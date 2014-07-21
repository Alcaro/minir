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

static const char * udevpath="/dev/input/by-id/";

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

struct inputkb_udev {
	struct inputkb i;
	
	//fd[0] is the inotify instance
	struct fdinfo * fd;
	unsigned int numfd;
	
	void (*key_cb)(struct inputkb * subject,
	               unsigned int keyboard, int scancode, int libretrocode,
	               bool down, bool changed, void* userdata);
	void* userdata;
};

static void fd_watch(struct inputkb_udev * this, int fd);
static void fd_unwatch(struct inputkb_udev * this, unsigned int id);



static void fd_activity(struct inputkb_udev * this, int fd)
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
printf("ev=%i %.8X %.8X %i %s\n",ev->wd,ev->mask,ev->cookie,ev->len,ev->name);
				ptr += sizeof(struct inotify_event)+ev->len;
			}
		}
	}
	else
	{
static int j=0;j++;
		struct input_event ev;
		//https://www.kernel.org/doc/Documentation/input/input.txt says
		//"and you'll always get a whole number of input events on a read"
		//their size is constant, no need to do anything weird
		while (read(fd, &ev, sizeof(ev)) > 0)
		{
printf("HI %i %i %i,%i,%i\n",fd,j,ev.type,ev.code,ev.value);
		}
	}
}

#ifdef GLIB
static gboolean fd_activity_glib(gint fd, GIOCondition condition, gpointer user_data)
{
	struct inputkb_udev * this=(struct inputkb_udev*)user_data;
	fd_activity(this, fd);
	return TRUE;//FALSE if the source should be removed
}
#endif

//static void fd_
//epoll_create1(0);

static void fd_watch(struct inputkb_udev * this, int fd)
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

static void fd_unwatch(struct inputkb_udev * this, unsigned int id)
{
#ifdef GLIB
	g_source_remove(this->fd[id].watchid);
#else
#error
#endif
	this->fd[id].state=0;
}

static void set_callback(struct inputkb * this_,
                         void (*key_cb)(struct inputkb * subject,
                                        unsigned int keyboard, int scancode, int libretrocode, 
                                        bool down, bool changed, void* userdata),
                         void* userdata)
{
	struct inputkb_udev * this=(struct inputkb_udev*)this_;
	this->key_cb=key_cb;
	this->userdata=userdata;
}

static void poll(struct inputkb * this)
{
#ifdef GLIB
	//do nothing - we're polled through the gtk+ main loop
#else
int epoll_wait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout);
#error
#endif
}

static void free_(struct inputkb * this_)
{
	struct inputkb_udev * this=(struct inputkb_udev*)this_;
	for (unsigned int i=0;i<this->numfd;i++)
	{
		if (this->fd[i].state)
		{
			close(this->fd[i].fd);
			fd_unwatch(this, i);
		}
	}
	free(this->fd);
	free(this);
}

struct inputkb * inputkb_create_udev(uintptr_t windowhandle)
{
return NULL;
	struct inputkb_udev * this=malloc(sizeof(struct inputkb_udev));
	this->i.set_callback=set_callback;
	this->i.poll=poll;
	this->i.free=free_;
	
	this->fd=NULL;
	this->numfd=0;
	
	int inotify=inotify_init1(IN_NONBLOCK);
	if (inotify<0) goto cancel;
	fd_watch(this, inotify);
	inotify_add_watch(inotify, udevpath, IN_CREATE);
//fd_watch(this, open("/dev/input/by-id/usb-LITEON_Technology_USB_Multimedia_Keyboard-event-kbd", O_RDONLY|O_NONBLOCK));
	
	return (struct inputkb*)this;
	
cancel:
	free_((struct inputkb*)this);
	return NULL;
}
#endif
