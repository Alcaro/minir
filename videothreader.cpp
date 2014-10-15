#include "minir.h"
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
//Yes means the action allowed in all cases; No means not allowed; Wakelock means allowed only if holding the mutex.
//Child-write and child-only is also possible, with the obvious meanings.
class video_thread : public video {
public://since this entire file is private, making it public inside here does no harm
	video* next;//Child-only.
	mutex* lock;//Thread safe.
	event* wake_parent;//Thread safe.
	event* wake_child;//Thread safe.
	uint8_t depth;//Read-only.
	uint8_t bpp;//Read-only.
	
	//char padding[2];
	
	unsigned int base_width;//Read-only.
	unsigned int base_height;//Read-only.
	
	unsigned int width;//Shared (external).
	unsigned int height;//Shared (external).
	bool update_size;//Shared (external).
	
	bool exit;//Shared. Setting this means the child should terminate.
	
	bool draw;//Shared. Setting this means the child should draw a new frame.
	bool draw_null;//Shared. Setting this tells the child to not fetch the new frame.
	bool draw_idle;//Shared. Setting this means the parent is allowed to proceed.
	
	//char padding[3];
	
	double vsync;//Parent-write. Setting this nonzero means the parent will wait for the child to finish drawing before itself claiming drawing is done.
	double new_vsync;//Shared (external). Must be separate from the above, because the parent must know whether a signal will come.
	//TODO: Screenshots
	//TODO: Shaders
	
	video_thread_frame buf_this;//Child-only. Currently being drawn to the screen.
	video_thread_frame buf_next;//Shared. Next to be drawn to the screen.
	video_thread_frame buf_temp;//Parent-only. Frame being created.
	//The above three sometimes swap their contents, at which point the access rules for each change as well.
	
	video_thread_frame buf_last;//Shared (external). Used for screenshot taking.
	
	uint32_t features() { return f_sshot_f|f_sshot|f_chain; }
	
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
		this->next->finalize_2d(this->base_width, this->base_height);
		this->wake_parent->signal();
		double vsync=0;
		while (true)
		{
			this->wake_child->wait();
			this->lock->lock();
			
			bool exit=this->exit;
			bool draw=this->draw;
			bool draw_null=this->draw_null;
			bool set_vsync=(vsync!=this->vsync);
			bool set_size=this->update_size;
			unsigned int width=this->width;
			unsigned int height=this->height;
			vsync=this->vsync;
			this->exit=false;
			this->draw=false;
			this->update_size=false;
			
			video_thread_frame buf;
			if (draw && !draw_null)
			{
				buf=this->buf_next;
				this->buf_next=this->buf_this;
				this->buf_this=buf;
			}
			
			this->lock->unlock();
			
			if (set_vsync) this->next->set_vsync(vsync);
			
			if (set_size) this->next->set_size(width, height);
			
			if (draw_null) this->next->draw_repeat();
			else this->next->draw_2d(buf.width, buf.height, buf.data, this->bpp*buf.width);
			
			if (vsync!=0)
			{
				this->lock->lock();
				this->draw_idle=true;
				this->lock->unlock();
				this->wake_parent->signal();
			}
			
			if (exit)
			{
				this->wake_parent->signal();
				return;
			}
		}
	}
	
	/*private*/ uint8_t depthtobpp(uint8_t depth)
	{
		if (depth==15) return 2;
		if (depth==16) return 2;
		if (depth==32) return 4;
		return 0;
	}
	
	void finalize_2d(unsigned int base_width, unsigned int base_height)
	{
		this->base_width=base_width;
		this->base_height=base_height;
		this->width=base_width;
		this->height=base_height;
		this->lock=new mutex();
		this->wake_child=new event();
		this->wake_parent=new event();
		thread_create(bind_this(&video_thread::threadproc));
		this->wake_parent->wait();
		this->exit=false;
		this->draw=false;
		this->draw_idle=true;
		this->vsync=false;
		this->new_vsync=false;
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
		size_t bytes=this->bpp*width*height;
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
		*pitch=this->bpp*width;
	}
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		assure_bufsize(width, height);
		this->buf_temp.width=width;
		this->buf_temp.height=height;
		if (data!=this->buf_temp.data)
		{
			video_copy_2d(this->buf_temp.data, this->bpp*width, data, pitch, this->bpp*width, height);
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
	
	void get_base_size(unsigned int * width, unsigned int * height)
	{
		if (width) *width=this->base_width;
		if (height) *height=this->base_height;
	}
	
	void set_size(unsigned int width, unsigned int height)
	{
		this->lock->lock();
		this->width=width;
		this->height=height;
		this->update_size=true;
		this->lock->unlock();
	}
	
	//void set_output(unsigned int screen_width, unsigned int screen_height);
	void set_chain(video* backend) { this->next=backend; }
	
	int get_screenshot(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth,
	                   void* * data, size_t datasize)
	{
		//this protects not against the child thread, but against the parent thread - screenshot/shader/vsync functions can be called by a third thread
		this->lock->lock();
		if (width) *width=this->buf_last.width;
		if (height) *height=this->buf_last.height;
		if (pitch) *pitch=this->bpp*this->buf_last.width;
		if (depth) *depth=this->depth;
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
	video_thread(unsigned int depth)
	{
		this->lock=NULL;
		this->buf_next.data=NULL;
		this->buf_next.bufsize=0;
		this->buf_this.data=NULL;
		this->buf_this.bufsize=0;
		this->buf_temp.data=NULL;
		this->buf_temp.bufsize=0;
		this->buf_last.data=NULL;
		this->buf_last.bufsize=0;
		this->depth=depth;
		this->bpp=depthtobpp(depth);
	}
	
	~video_thread()
	{
		if (!this->lock) return;
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

video* video_create_thread(unsigned int depth)
{
	return new video_thread(depth);
}
