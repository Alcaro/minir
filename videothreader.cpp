#include "io.h"
#include "os.h"
#include <stdlib.h>
#include <string.h>

namespace {

struct video_thread_frame {
	uint8_t* data;
	size_t bufsize;
	unsigned int width;
	unsigned int height;
};

//Variables may be declared in the following ways:
//             | Child read | Child write | Parent read | Parent write |
//Shared       | Lock       | Lock        | Lock        | Lock         |
//Parent-write | Lock       | No          | Yes         | Lock         |
//Parent-only  | No         | No          | Yes         | Yes          |
//Read-only    | Yes        | No          | Yes         | No           |
//Yes means the action allowed in all cases; No means not allowed; Lock means allowed only if holding the mutex.
//Child-write and child-only is also possible, with the obvious meanings.
class video_thread : public video {
public://since this entire file is private, making it public inside here does no harm
	video* next;//Child-only.
	mutex* lock;//Thread safe.
	event* wake_parent;//Thread safe.
	event* wake_child;//Thread safe.
	
	//src and dest are used instead of src/dst because they have different length and are therefore easier to identify
	unsigned int src_width;//Shared (external).
	unsigned int src_height;//Shared (external).
	uint8_t src_depth;//Shared (external).
	uint8_t src_bpp;//Shared (external).
	//char padding[2];
	
	unsigned int dest_width;//Shared (external).
	unsigned int dest_height;//Shared (external).
	
	bool exit;//Shared. Setting this means the child should terminate.
	
	bool draw;//Shared. Setting this means the child should draw a new frame.
	bool draw_null;//Shared. Setting this tells the child to not fetch the new frame.
	bool draw_idle;//Shared. Setting this means the parent is allowed to proceed.
	
	double vsync;//Parent-write. Setting this nonzero means the parent will wait for the child to finish drawing before itself claiming drawing is done.
	double new_vsync;//Shared (external). Must be separate from the above, because the parent must know whether a signal will come.
	//TODO: Screenshots
	//TODO: Shaders
	
	video_thread_frame buf_this;//Child-only. Currently being drawn to the screen.
	video_thread_frame buf_next;//Shared. Next to be drawn to the screen.
	video_thread_frame buf_temp;//Parent-only. Frame being created.
	//The above three sometimes swap their contents, at which point the access rules for each change as well.
	
	video_thread_frame buf_last;//Shared (external). Used for screenshot taking.
	
	uint32_t features() { return f_sshot_ptr|f_sshot|f_chain; }
	
	//When the child thread is done drawing:
	//- lock->lock_wait()
	//- Swap buf_next and buf_this
	//- Set buf_next_full false
	//- lock->unlock()
	//- Draw buf_this
	
	//When the parent wants to draw something:
	//- Copy to buf_temp, ensure it's packed (reallocate if needed)
	//- Clone buf_temp to buf_last
	//- If sync: lock->lock_wait()
	//- Else: lock->lock()
	//- Swap buf_next and buf_this, whether buf_next_full is set or not
	//- Set buf_next_full
	//- lock->unlock()
	
	/*private*/ void threadproc()
	{
		this->next->initialize();
		unsigned int src_width=0;
		unsigned int src_height=0;
		uint8_t src_depth=0;
		unsigned int dest_width=0;
		unsigned int dest_height=0;
		//not calling set_source and set_dest here
		this->wake_parent->signal();
		double vsync=0;
		while (true)
		{
			this->wake_child->wait();
			this->lock->lock();
			
			if (this->exit)
			{
				delete this->next;
				this->exit=false;
				this->wake_parent->signal();
				this->lock->unlock();
				return;
			}
			
			bool draw=this->draw;
			this->draw=false;
			bool draw_null=this->draw_null;
			
			bool set_vsync=(vsync!=this->vsync);
			vsync=this->vsync;
			
			bool set_src=(src_width!=this->src_width || src_height!=this->src_height || src_depth!=this->src_depth);
			src_width=this->src_width;
			src_height=this->src_height;
			src_depth=this->src_depth;
			
			bool set_dest=(dest_width!=this->dest_width || dest_height!=this->dest_height);
			dest_width=this->dest_width;
			dest_height=this->dest_height;
			
			video_thread_frame buf;
			if (draw && !draw_null)
			{
				buf=this->buf_next;
				this->buf_next=this->buf_this;
				this->buf_this=buf;
			}
			
			this->lock->unlock();
			
			if (set_src) this->next->set_source(src_width, src_height, (videoformat)src_depth);
			if (set_dest) this->next->set_dest_size(dest_width, dest_height);
			
			if (set_vsync) this->next->set_vsync(vsync);
			
			if (draw_null) this->next->draw_repeat();
			else this->next->draw_2d(buf.width, buf.height, buf.data, this->src_bpp*buf.width);
			
			if (vsync!=0)
			{
				this->lock->lock();
				this->draw_idle=true;
				this->lock->unlock();
				this->wake_parent->signal();
			}
		}
	}
	
	/*private*/ void initialize()
	{
		thread_create(bind_this(&video_thread::threadproc));
		this->wake_parent->wait();
	}
	
	/*private*/ uint8_t depthtobpp(uint8_t depth)
	{
		if (depth==fmt_xrgb8888) return 4;
		else return 2;
	}
	
	void set_chain(video* next) { this->next=next; }
	
	void set_source(unsigned int max_width, unsigned int max_height, videoformat depth)
	{
		this->lock->lock();
		this->src_depth=depth;
		this->src_bpp=depthtobpp(depth);
		this->src_width=max_width;
		this->src_height=max_height;
		this->lock->unlock();
	}
	
	/*private*/ void draw_frame(bool real_frame)
	{
		this->lock->lock();
		if (real_frame)
		{
			this->buf_last=this->buf_temp;
			this->buf_temp=this->buf_next;
			this->buf_next=this->buf_last;
		}
		this->draw=true;
		this->draw_null=!real_frame;
		this->draw_idle=false;
		this->vsync=this->new_vsync;
		this->lock->unlock();
		this->wake_child->signal();
		if (this->vsync!=0)
		{
			while (true)
			{
				this->wake_parent->wait();
				this->lock->lock();
				bool done = this->draw_idle;
				this->lock->unlock();
				if (done) break;
			}
		}
	}
	
	/*private*/ void assure_bufsize(unsigned int width, unsigned int height)
	{
		size_t bytes=this->src_bpp*width*height;
		if (bytes > this->buf_temp.bufsize)
		{
			free(this->buf_temp.data);
			this->buf_temp.data=malloc(bytes);
			this->buf_temp.bufsize=bytes;
		}
	}
	
	void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch)
	{
		assure_bufsize(width, height);
		*data=this->buf_temp.data;
		*pitch=this->src_bpp*width;
	}
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		assure_bufsize(width, height);
		this->buf_temp.width=width;
		this->buf_temp.height=height;
		if (data!=this->buf_temp.data)
		{
			video_copy_2d(this->buf_temp.data, this->src_bpp*width, data, pitch, this->src_bpp*width, height);
		}
		draw_frame(true);
	}
	
	//bool set_input_3d(struct retro_hw_render_callback* input3d);
	//uintptr_t input_3d_get_current_framebuffer();
	//funcptr input_3d_get_proc_address(const char *sym);
	//void draw_3d(unsigned int width, unsigned int height);
	
	void draw_repeat()
	{
		draw_frame(false);
	}
	
	void set_vsync(double fps)
	{
		//this protects not against the child thread, but against the parent thread - screenshot/shader/vsync functions can be called by a third thread
		this->lock->lock();
		this->new_vsync=fps;
		this->lock->unlock();
	}
	
	//TODO: Enable those.
	//virtual bool set_shader(shadertype type, const char * filename) { return false; }
	//virtual video_shader_param* get_shader_params() { return NULL; }
	//virtual void set_shader_param(unsigned int index, double value) {}
	
	void set_dest_size(unsigned int width, unsigned int height)
	{
		this->lock->lock();
		this->dest_width=width;
		this->dest_height=height;
		this->lock->unlock();
	}
	
	int get_screenshot(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth,
	                   void* * data, size_t datasize)
	{
		//this protects not against the child thread, but against the parent thread - screenshot/shader/vsync functions can be called by a third thread
		this->lock->lock();
		if (width) *width=this->buf_last.width;
		if (height) *height=this->buf_last.height;
		if (pitch) *pitch=this->src_bpp*this->buf_last.width;
		if (depth) *depth=this->src_depth;
		if (data) *data=this->buf_last.data;
		return 1;
	}
	
	//TODO: Enable.
	//int get_screenshot_out(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth,
	//                       void* * data, size_t datasize);
	
	void release_screenshot(int ret, void* data)
	{
		if (!ret) return;
		this->lock->unlock();
	}
	
	video_thread()
	{
		this->src_depth=0;
		this->src_bpp=0;
		this->src_width=0;
		this->src_height=0;
		this->dest_width=0;
		this->dest_height=0;
		this->lock=new mutex();
		this->wake_child=new event();
		this->wake_parent=new event();
		
		this->exit=false;
		this->draw=false;
		this->draw_idle=true;
		this->vsync=0;
		this->new_vsync=0;
		
		this->buf_next.data=NULL;
		this->buf_next.bufsize=0;
		this->buf_this.data=NULL;
		this->buf_this.bufsize=0;
		this->buf_temp.data=NULL;
		this->buf_temp.bufsize=0;
		this->buf_last.data=NULL;
		this->buf_last.bufsize=0;
	}
	
	~video_thread()
	{
		this->lock->lock();
		this->exit=true;
		this->lock->unlock();
		this->wake_child->signal();
		while (true)
		{
			this->wake_parent->wait();
			this->lock->lock();
			bool exit=(!this->exit);
			this->lock->unlock();
			if (exit) break;
		}
		delete this->lock;
		delete this->wake_child;
		delete this->wake_parent;
		free(this->buf_this.data);
		free(this->buf_next.data);
		free(this->buf_temp.data);
	}
};

}

video* video_create_thread()
{
	return new video_thread();
}
