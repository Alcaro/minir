#include "minir.h"
#ifdef INPUT_UDEV
#include <sys/inotify.h>
#include <linux/input.h>
#include <string.h>

#ifdef WINDOW_GTK3
#define GLIB
#endif

#ifdef GLIB
#include <glib.h>
#include <glib-unix.h>
#else
#error
#endif

namespace {

static const char * udevpath="/dev/input/";

#define test_bit(buf, bit) ((buf)[(bit)>>3] & 1<<((bit)&7))

struct fdinfo {
	int fd;
	char state;//0 - free (can be reused)
	           //1 - used but inactive (no allocated keyboard ID; can be moved)
	           //2 - active (must stay there)
#ifdef GLIB
	guint watchid;
#else
#error
#endif
};

#ifdef GLIB
#define GLIB_N 1
#else
#define GLIB_N 0
#endif

class inputkb_udev : public inputkb {
public:
	//fd[0] is the inotify instance
	struct fdinfo * fd;
	unsigned int numfd;
	
public:
	int linuxcode_to_scan(int fd, unsigned int code)
	{
		if (code<=255) return code+8;//I have zero clue where this 8 comes from.
		else return -1;
	}
	void fd_watch(int fd);
	void fd_unwatch(unsigned int id);
	
	unsigned int alloc_id(int fd);
	
	bool openpath(const char * fname);
	void fd_activity(int fd);
	
public:
	inputkb_udev() {}
	bool construct(uintptr_t windowhandle);
	
	uint32_t features() { return f_multi|f_delta|(GLIB_N ? 0 : f_auto)|f_direct|f_background|f_pollable; }
	
	void refresh();
#ifndef GLIB
	void poll(); // we do this through the gtk+ main loop if we can, but if there is no gtk+ main loop, we can poll
#endif
	
	~inputkb_udev();
};

unsigned int inputkb_udev::alloc_id(int fd)
{
	//allocate IDs as lazily as possible - we don't want to hand out a keyboard ID because the mouse is doing something.
	unsigned int id;
	for (id=0;fd!=this->fd[id].fd;id++) {}
	if (this->fd[id].state==1)
	{
		unsigned int firstfree;
		for (firstfree=0;this->fd[firstfree].state==2;firstfree++) {}//this won't go out of bounds - we can hit ourselves
		
		struct fdinfo tmp=this->fd[firstfree];
		this->fd[firstfree]=this->fd[id];
		this->fd[id]=tmp;
		id=firstfree;
		this->fd[id].state=2;
	}
	return id;
}

//return value is whether we have access to this device; it's true even if the device is rejected for not being a keyboard
bool inputkb_udev::openpath(const char * fname)
{
	char path[512];
	strcpy(path, udevpath); strcat(path, fname);
	int fd=open(path, O_RDONLY|O_NONBLOCK);
	if (fd<0) return false;
	
	uint8_t events[EV_MAX];
	memset(events, 0, sizeof(events));
	ioctl(fd, EVIOCGBIT(0, EV_MAX), events);
	
	if (!test_bit(events, EV_KEY))//trim off anything that doesn't have any buttons (still captures mouse buttons though...)
	{
		close(fd);
		return true;
	}
	
	this->fd_watch(fd);
	
	//for (int i=0;i<256;i++)
	//{
	//	struct input_keymap_entry km;
	//	memset(&km, 0, sizeof(km));
	//	km.flags=INPUT_KEYMAP_BY_INDEX;
	//	km.index=i;
	//	
	//	if (!ioctl(fd, EVIOCGKEYCODE_V2, &km))
	//	{
	//		printf("%s/%.2X: %.2X %.2X %.4X %.8X ", fname,i, km.flags,km.len,km.index,km.keycode);
	//		for (unsigned int i=0;i<km.len;i++) printf("%.2X",km.scancode[i]);
	//		puts("");
	//	}
	//}
	
//	for (unsigned int bit=0;bit<EV_MAX;bit++)
//	{
//		if (events[bit>>3] & 1<<(bit&7))
//		{
//			printf("support %i\n",bit);
//		}
//	}
//
//for (yalv = 0; yalv < ; yalv++) {
//    if (test_bit(yalv, evtype_b)) {
//	memset(evtype_b, 0, sizeof(evtype_b));
//	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
//	    perror("evdev ioctl");
//	}
	
	return true;
}



void inputkb_udev::fd_activity(int fd)
{
	if (fd == this->fd[0].fd)
	{
		char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
		while (true)
		{
			ssize_t len=read(fd, buf, sizeof(buf));
			if (len<=0) break;
			
			char* ptr=buf;
			while (ptr < buf+len)
			{
				const struct inotify_event* ev=(struct inotify_event*)ptr;
				this->openpath(ev->name);
//printf("ev=%i %.8X %.8X %i %s\n",ev->wd,ev->mask,ev->cookie,ev->len,ev->name);
				ptr += sizeof(struct inotify_event)+ev->len;
			}
		}
	}
	else
	{
		int id=-1;
		
		struct input_event ev;
		//https://www.kernel.org/doc/Documentation/input/input.txt says
		//"and you'll always get a whole number of input events on a read"
		//their size is constant, no need to do anything weird
		while (read(fd, &ev, sizeof(ev)) > 0)
		{
			if (ev.type==EV_KEY)
			{
				int scan=linuxcode_to_scan(fd, ev.code);
				if (scan<0) continue;
				if (id==-1) id=this->alloc_id(fd);
//printf("evc=%.2X sc=%.2X\n",ev.code,scan);
				this->key_cb(id-1, scan, inputkb_translate_scan(scan), (ev.value!=0));//ev.value==2 means repeated
			}
		}
	}
}

#ifdef GLIB
static gboolean fd_activity_glib(gint fd, GIOCondition condition, gpointer user_data)
{
	inputkb_udev* obj=(inputkb_udev*)user_data;
	obj->fd_activity(fd);
	return TRUE;//FALSE if the source should be removed
}
#endif

//static void fd_
//epoll_create1(0);

void inputkb_udev::fd_watch(int fd)
{
	unsigned int id=0;
	while (id<this->numfd && this->fd[id].state) id++;
	if (id==this->numfd)
	{
		this->fd=realloc(this->fd, sizeof(struct fdinfo)*(++this->numfd));
	}
	this->fd[id].fd=fd;
	this->fd[id].state=1;
#ifdef GLIB
	this->fd[id].watchid=g_unix_fd_add(fd, G_IO_IN/*|G_IO_HUP*/, fd_activity_glib, this);
#else
#error
#endif
}

void inputkb_udev::fd_unwatch(unsigned int id)
{
#ifdef GLIB
	g_source_remove(this->fd[id].watchid);
#else
#error
#endif
	this->fd[id].state=0;
	this->fd[id].fd=-1;
}

void inputkb_udev::refresh()
{
	for (unsigned int id=1;id<this->numfd;id++)
	{
		uint8_t keys[KEY_MAX/8 + 1];
		memset(keys, 0, sizeof(keys));
		ioctl(this->fd[id].fd, EVIOCGKEY(sizeof(keys)), keys);
		
		for (unsigned int bit=0;bit<KEY_MAX;bit++)
		{
			if (test_bit(keys, bit))
			{
				int scan=this->linuxcode_to_scan(this->fd[id].fd, bit);
				if (scan<0) continue;
				unsigned int kbid=this->alloc_id(this->fd[id].fd);
				this->key_cb(kbid-1, scan, inputkb_translate_scan(scan), true);
			}
		}
	}
}

#ifndef GLIB
//if glib, do nothing - we're polled through the gtk+ main loop
void inputkb_udev::poll()
{
int epoll_wait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout);
#error
}
#endif

inputkb_udev::~inputkb_udev()
{
	for (unsigned int i=0;i<this->numfd;i++)
	{
		if (this->fd[i].state)
		{
			close(this->fd[i].fd);
			this->fd_unwatch(i);
		}
	}
	free(this->fd);
}

bool inputkb_udev::construct(uintptr_t windowhandle)
{
	this->fd=NULL;
	this->numfd=0;
	
	int inotify=inotify_init1(IN_NONBLOCK);
	if (inotify<0) goto cancel;
	this->fd_watch(inotify);
	this->fd[0].state=2;
	inotify_add_watch(inotify, udevpath, IN_CREATE);
	
	bool access; access=false;
	while (true)
	{
		DIR* dir=opendir(udevpath);
		while (true)
		{
			struct dirent * ent=readdir(dir);
			if (!ent) break;
			if (ent->d_type==DT_DIR) continue;
			access|=this->openpath(ent->d_name);
		}
		closedir(dir);
		
		//if anything happened, let's just throw it out and try again because we don't know if we got that file open.
		char buf[4096];
		ssize_t len=read(this->fd[0].fd, buf, sizeof(buf));
		if (len<0) break;
		for (unsigned int i=0;i<this->numfd;i++)
		{
			if (this->fd[i].state)
			{
				close(this->fd[i].fd);
				this->fd_unwatch(i);
			}
		}
	}
	
	if (!access) goto cancel;//couldn't open any devices? we probably can't access them, let's abort.
	                         //this could be due to not having any input device plugged in, but that's not likely.
	
	return true;
	
cancel:
	return false;
}

}

inputkb* inputkb_create_udev(uintptr_t windowhandle)
{
	inputkb_udev* obj=new inputkb_udev();
	if (!obj->construct(windowhandle))
	{
		delete obj;
		return NULL;
	}
	return obj;
}
#endif
