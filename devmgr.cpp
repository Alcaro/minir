#include "minir.h"
#include <string.h>

class devmgr::impl : public devmgr {
public:

struct devicedat {
	device* obj;
	uint32_t events[2];//[0]=primary, [1]=secondary (index with event->secondary)
};

event* ev_head;
event* ev_tail;

event* ev_head_thread;

//[0] is the core, [1] is the primary device
struct devicedat * devices;
size_t numdevices;


struct buttondat {
	device* dev;
	unsigned int id;
	bool hold;
	//char padding[3];
};
inputmapper* mapper;
array<buttondat> buttons;


void ev_append(event* ev, device* source)
{
	ev->source=source;
	if (this->ev_tail) this->ev_tail->next=ev;
	else this->ev_head=ev;
	this->ev_tail=ev;
}

void ev_append_thread(event* ev, device* source)
{
	ev->source=source;
	
	event* oldval=lock_read(&this->ev_head_thread);
	while (true)
	{
		ev->next=oldval;
		event* newval=lock_write_eq(&this->ev_head_thread, oldval, ev);
		if (newval==oldval) break;
		else oldval=newval;
	}
}

//void ev_release_thread(event* ev, device* source)
//{
//	ev->source=source;
//	
//	event* oldval=lock_read(&this->ev_head_thread);
//	while (true)
//	{
//		ev->next=oldval;
//		event* newval=lock_write_eq(&this->ev_head_thread, oldval, ev);
//		if (newval==oldval) break;
//		else oldval=newval;
//	}
//}

/*private*/ void ev_do_dispatch(event* ev, device* dev)
{
	switch (ev->type)
	{
#define HANDLE_EVENT(name) \
	case event::ty_##name: \
		dev->ev_##name((event::name*)ev); \
		break
	//ignore null events, they're never dispatched anyways
	HANDLE_EVENT(frame);
	HANDLE_EVENT(state_save);
	HANDLE_EVENT(state_load);
	HANDLE_EVENT(video);
	HANDLE_EVENT(audio);
	HANDLE_EVENT(keyboard);
	HANDLE_EVENT(mousemove);
	HANDLE_EVENT(mousebutton);
	HANDLE_EVENT(gamepad);
	HANDLE_EVENT(button);
#undef HANDLE_EVENT
	}
}

//Returns whether the event was dispatched.
/*private*/ bool ev_dispatch_to(event* ev, size_t index)
{
	static const uint16_t subscriber_bits[]={
		0,
		e_frame, e_savestate, e_savestate,
		e_video, e_audio,
		e_keyboard, e_mouse, e_mouse, e_gamepad,
	};
	
	if (ev->source == this->devices[index].obj) return false;
	
	if (subscriber_bits[ev->type] & this->devices[index].events[ev->secondary])
	{
		//no need to null check obj; null objs ask for no events, so they won't get here
		ev_do_dispatch(ev, this->devices[index].obj);
		return true;
	}
	return false;
}

void ev_dispatch_sec(event* ev)
{
	for (size_t i=2;i<this->numdevices;i++)
	{
		ev_dispatch_to(ev, i);
	}
	ev_dispatch_to(ev, 0);
}

/*private*/ void ev_dispatch(event* ev)
{
	bool sent=ev_dispatch_to(ev, 1);
	if (!sent) ev_dispatch_sec(ev);
}

/*private*/ void ev_dispatch_chain(event* ev)
{
	while (ev)
	{
		ev_dispatch(ev);
		event* next=ev->next;
		ev->next=NULL;
		ev->release();
		ev=next;
	}
}

/*private*/ void ev_dispatch_all()
{
	ev_dispatch_chain(this->ev_head);
	this->ev_head=NULL;
	this->ev_tail=NULL;
}

/*private*/ void ev_button(unsigned int id, bool down)
{
	event::button ev;
	ev.id = buttons[id].id;
	ev.down = down;
	buttons[id].dev->ev_button(&ev);
}


void dev_register_events(device* target, uint32_t primary, uint32_t secondary)
{
	size_t i=0;
	while (true)
	{
		if (this->devices[i].obj == target)
		{
			this->devices[i].events[0]=primary;
			this->devices[i].events[1]=secondary;
			break;
		}
		i++;
	}
}

bool dev_register_button(device* target, const char * desc, unsigned int id, bool hold)
{
	int newid = mapper->register_button(desc);
	if (newid < 0) return false;
	buttons[newid].dev = target;
	buttons[newid].id = id;
	return true;
}

bool dev_test_button(device* target, unsigned int id)
{
	//TODO
	return false;
}

void dev_unregister(device* dev)
{
	size_t i=1;
	while (true)
	{
		if (this->devices[i].obj == dev)
		{
			this->devices[i].obj=NULL;
			this->devices[i].events[0]=0;
			this->devices[i].events[1]=0;
			dev->detach();
			dev->parent=NULL;
			
			for (size_t j=0;j<buttons.len();j++)
			{
				if (buttons[j].dev == dev)
				{
					buttons[j].dev = NULL;
					mapper->register_button(j, NULL);
				}
			}
			//TODO: unregister button events
			break;
		}
		i++;
	}
}


uint8_t * state_save(size_t* size)
{
	return NULL;
}

bool state_load(const uint8_t * data, size_t size)
{
	return false;
}


void frame()
{
	//Event dispatch order:
	//The primary frame must be the first event all devices get. Events created by that are delayed until this event is processed by everything.
	//The secondary frame must be the last event all devices get, except core output. 
	//1. The primary frame is sent to all devices. This must be the first event they get.
	//2. All events created by that, as well as the thread-submitted events, are processed.
	//3. The secondary frame is sent to the primary device.
	//4. Events created by that are sent before  However, events it creates are processed before the secondary frame is sent to the other devices.
	//
	//event dispatch order:
	//- primary frame to primary device
	//- primary frame to secondary devices
	//- threaded event queue (dispatched backwards because it's easier - I could flip it, but why should I?)
	//- event queue
	//- secondary frame to primary device
	//- event queue again
	//- secondary frame to secondary devices, including core
	//- event queue again (core output)
	event::frame ev;
	//ev.hold();
	
	ev.secondary=false;
	ev_dispatch_to(&ev, 1);
	ev_dispatch_sec(&ev);
	ev_dispatch_chain(lock_xchg(&this->ev_head_thread, NULL));
	ev_dispatch_all();
	
	ev.secondary=true;
	ev_dispatch_to(&ev, 1);
	ev_dispatch_all();
	ev_dispatch_sec(&ev);
	
	ev_dispatch_all();
}

bool add_device(device* dev)
{
	dev->parent=this;
	size_t i;
	
	device::devtype type=dev->type();
	if (type==device::t_primary)
	{
		i=1;
	}
	else if (type==device::t_core)
	{
		i=0;
	}
	else
	{
		//not primary nor core - find or create a free slot
		for (i=2;i<this->numdevices;i++)
		{
			if (this->devices[i].obj==NULL) break;
		}
		if (i == this->numdevices)
		{
			if ((this->numdevices & (this->numdevices-1)) == 0)
			{
				this->devices=realloc(this->devices, sizeof(struct devicedat)*this->numdevices*2);
			}
			this->numdevices++;
		}
	}
	this->devices[i].obj=dev;
	this->devices[i].events[0]=0;
	this->devices[i].events[1]=0;
	dev->attach();
	return true;
}


impl()
{
	this->numdevices=2;
	this->devices=malloc(sizeof(struct devicedat)*this->numdevices);
	memset(this->devices, 0, sizeof(struct devicedat)*this->numdevices);
	
	this->ev_head=NULL;
	this->ev_tail=NULL;
	this->ev_head_thread=NULL;
	
	this->mapper=inputmapper::create();
	this->mapper->set_cb(bind_this(&impl::ev_button));
}

~impl()
{
	for (size_t i=0;i<this->numdevices;i++)
	{
		delete this->devices[i].obj;
	}
}

};

devmgr* devmgr::create() { return new devmgr::impl(); }
