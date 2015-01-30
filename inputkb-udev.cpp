#include "io.h"
#ifdef INPUT_UDEV
#include <sys/inotify.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <string.h>
//#include <sys/types.h>
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

#define udevpath "/dev/input/"

#define test_bit(buf, bit) ((buf)[(bit)>>3] & 1<<((bit)&7))

enum { ty_kb, ty_mouse, ty_last, ty_free=0xFF, ty_special=0xFF };//special can only exist on inputkb_udev::fd[0]
struct fdinfo {
	int fd;
	uint8_t type;//255 - special (inotify)
	             //0 - keyboard
	             //1 - mouse
	             //(more to come)
	uint8_t id;//index to fd_for_type
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
	uint8_t numfd;
	//char padding[5];
	
	uint8_t n_of_type[ty_last];
	uint8_t* fd_for_type[ty_last];
	
public:
	int linuxcode_to_scan(int fd, unsigned int code)
	{
		if (code<=255) return code+8;//I have zero clue where this 8 comes from.
		else return -1;
	}
	void fd_watch(int fd, uint8_t type);
	void fd_unwatch(unsigned int id);
	
	uint8_t alloc_id_for(uint8_t type);
	
	bool openpath(const char * fname);
	void fd_activity(int fd);
	
public:
	inputkb_udev() {}
	bool construct(uintptr_t windowhandle);
	
	static const uint32_t feat = f_multi|f_delta|(GLIB_N ? 0 : f_auto)|f_direct|f_background|f_pollable;
	uint32_t features() { return feat; }
	
	void refresh();
#ifndef GLIB
	void poll(); // we do this through the gtk+ main loop if we can, but if there is no gtk+ main loop, we'll have to poll
#endif
	
	~inputkb_udev();
};

uint8_t inputkb_udev::alloc_id_for(uint8_t type)
{
//for(int i=0;i<this->n_of_type[type];i++)printf("/%.2X/",this->fd_for_type[type][i]);puts("#");
	unsigned int id=0;
	while (id<this->n_of_type[type] && this->fd_for_type[type][id]!=ty_free) id++;
	if (id==this->n_of_type[type])
	{
		unsigned int newlen;
		if (id==0) newlen=4;
		else newlen=id*2;
		this->n_of_type[type]=newlen;
		this->fd_for_type[type]=realloc(this->fd_for_type[type], sizeof(uint8_t)*newlen);
		for (unsigned int i=id;i<newlen;i++) this->fd_for_type[type][i]=ty_free;
	}
	return id;
}

//return value is whether we have access to this device; it's true even if the device is rejected for not being usable
bool inputkb_udev::openpath(const char * fname)
{
	char path[512];
	strcpy(path, udevpath); strcpy(path+strlen(udevpath), fname);
	int fd=open(path, O_RDONLY|O_NONBLOCK);
	if (fd<0) return false;
	
	//let's check if this one is anything we're interested in
	uint8_t events[EV_MAX];
	memset(events, 0, sizeof(events));
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), events) < 0)
	{
		//directories don't know what an EVIOCGBIT is
		close(fd);
		return false;
	}
	
	if (test_bit(events, EV_KEY))
	{
		memset(events, 0, 4);
		ioctl(fd, EVIOCGBIT(EV_KEY, 4), events);//the real max value is KEY_MAX, but we only want the first four bytes.
		if (events[0]>=0xFE && events[1]==0xFF && events[2]==0xFF && events[3]==0xFF)
		{
			//it's got ESC, numbers, and Q through D - it's a keyboard
			//(rule stolen from https://github.com/gentoo/eudev/blob/master/src/udev/udev-builtin-input_id.c#L186 )
			this->fd_watch(fd, ty_kb);
			return true;
		}
	}
	
	//not interested - however, we did manage to open an udev device, so we have access to them and can use this driver
	close(fd);
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
		unsigned int id;
		for (id=0;this->fd[id].fd!=fd;id++) {}
		
		struct input_event ev;
		//https://www.kernel.org/doc/Documentation/input/input.txt says
		//"and you'll always get a whole number of input events on a read"
		//their size is constant, no need to do anything weird
	another:
		ssize_t nbyte = read(fd, &ev, sizeof(ev));
		if (nbyte<=0)
		{
			for (unsigned int i=0;i<256;i++)//release all keys if the keyboad is unplugged
			{
				this->key_cb(this->fd[id].id, i, inputkb::translate_scan(i), false);
			}
			fd_unwatch(id);
		}
		if (ev.type==EV_KEY)
		{
			int scan=linuxcode_to_scan(fd, ev.code);
			if (scan<0) goto another;
//printf("evc=%.2X sc=%.2X\n",ev.code,scan);
			this->key_cb(this->fd[id].id, scan, inputkb::translate_scan(scan), (ev.value!=0));//ev.value==2 means repeated
			goto another;
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

void inputkb_udev::fd_watch(int fd, uint8_t type)
{
	unsigned int id=1;
	while (id<this->numfd && this->fd[id].type!=ty_free) id++;
	if (type==ty_special) id=0;
	if (id==this->numfd)
	{
		this->fd=realloc(this->fd, sizeof(struct fdinfo)*(++this->numfd));
	}
	this->fd[id].fd=fd;
	this->fd[id].type=type;
	
	if (type!=ty_special)
	{
		unsigned int type_id=alloc_id_for(type);
		this->fd[id].id=type_id;
		this->fd_for_type[type][type_id]=id;
	}
	
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
	this->fd[id].type=ty_free;
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
				this->key_cb(this->fd[id].id, scan, inputkb::translate_scan(scan), true);
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
	for (unsigned int i=1;i<this->numfd;i++)
	{
		if (this->fd[i].type!=ty_free)
		{
			int fd=this->fd[i].fd;
			this->fd_unwatch(i);
			close(fd);
		}
	}
	int fd=this->fd[0].fd;
	this->fd_unwatch(0);
	close(fd);
	free(this->fd);
	for (unsigned int i=0;i<ty_last;i++) free(this->fd_for_type[i]);
}

bool inputkb_udev::construct(uintptr_t windowhandle)
{
	this->fd=NULL;
	this->numfd=0;
	
	for (unsigned int i=0;i<ty_last;i++)
	{
		this->fd_for_type[i]=NULL;
		this->n_of_type[i]=0;
	}
	
	int inotify=inotify_init1(IN_NONBLOCK);
	if (inotify<0) goto cancel;
	this->fd_watch(inotify, ty_special);
	inotify_add_watch(inotify, udevpath, IN_CREATE);
	
	bool access; access=false;
	while (true)
	{
		DIR* dir=opendir(udevpath);
		//order seems to be reverse insertion order
		while (true)
		{
			struct dirent * ent=readdir(dir);
			if (!ent) break;
			//d_type can be DT_UNKNOWN and hide a directory, but that'll be blocked later because they don't respond to our ioctls.
			if (ent->d_type==DT_DIR) continue;
			access|=this->openpath(ent->d_name);
		}
		closedir(dir);
		
		//if anything happened, let's just throw it out and try again because we don't know if we got that file open.
		//There doesn't seem to be any way to check if two fds are equivalent.
		char buf[4096];
		ssize_t len=read(this->fd[0].fd, buf, sizeof(buf));
		if (len<0) break;
		for (unsigned int i=1;i<this->numfd;i++)
		{
			if (this->fd[i].fd!=-1)
			{
				int fd=this->fd[i].fd;
				this->fd_unwatch(i);
				close(fd);
			}
		}
	}
	
	if (!access) goto cancel;//if we can't access anything (not even non-devices), let's abort
	                         //there is a theoretical possibility that /dev/input/ is empty,
	                         //but in practice, everything has at least a Power Button.
	
	return true;
	
cancel:
	return false;
}

static inputkb* inputkb_create_udev(uintptr_t windowhandle)
{
	inputkb_udev* obj=new inputkb_udev();
	if (!obj->construct(windowhandle))
	{
		delete obj;
		return NULL;
	}
	return obj;
}

}

const inputkb::driver inputkb::driver_udev={ "udev", inputkb_create_udev, inputkb_udev::feat };
#endif
