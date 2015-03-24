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

uint8_t buttonblock;


void ev_append(event* ev)
{
	if (this->ev_tail) this->ev_tail->next=ev;
	else this->ev_head=ev;
	this->ev_tail=ev;
}

void ev_append_async(event* ev)
{
	event* oldval=lock_read(&this->ev_head_thread);
	while (true)
	{
		ev->next=oldval;
		event* newval=lock_write_eq(&this->ev_head_thread, oldval, ev);
		if (newval==oldval) break;
		else oldval=newval;
	}
}

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
	//ignore button events, they're handled specially
#undef HANDLE_EVENT
	}
}

//Returns whether the event was dispatched.
/*private*/ void ev_dispatch_to(event* ev, size_t index)
{
	static const uint16_t subscriber_bits[]={
		0,
		e_frame, e_savestate, e_savestate,
		e_video, e_audio,
		e_keyboard, e_mouse, e_mouse, e_gamepad,
	};
	
	if (ev->source != this->devices[index].obj &&
	    subscriber_bits[ev->type] & this->devices[index].events[ev->secondary])
	{
		//no need to null check obj; null objs ask for no events, so they won't get here
		ev_do_dispatch(ev, this->devices[index].obj);
	}
}

/*private*/ void ev_dispatch(event* ev)
{
	if (ev->blockstate != event::bs_resent) ev_dispatch_to(ev, 1);
	
	if (ev->type == event::ty_keyboard) // TODO: mousemove, mousebutton, gamepad
	{
		event::keyboard* key = (event::keyboard*)ev;
		buttonblock = key->blockstate;
		mapper->event(inputmapper::dev_kb, key->deviceid, key->libretrocode, key->scancode, key->down);
	}
	
	if (ev->blockstate == event::bs_blocked) return;
	for (size_t i=2;i<this->numdevices;i++)
	{
		ev_dispatch_to(ev, i);
	}
	ev_dispatch_to(ev, 0);
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

/*private*/ void ev_button(unsigned int id, inputmapper::triggertype events)
{
	event::button ev;
	ev.id = buttons[id].id;
	ev.down = events&5;//TODO: do this properly
printf("ev%i=%i\n",id,events);
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

bool dev_register_button(device* target, const char * desc, device::buttontype when, unsigned int id)
{
	//TODO: use buttontype
	int newid = mapper->register_button(desc);
printf("%s=%i\n",desc,newid);
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
	//1. The primary frame is sent to all devices. This is the first event they get.
	//2. All events created by that, as well as the asynchronously submitted events, are processed.
	//3. The secondary frame is sent to the primary device.
	//4. Events created by that are processed.
	//5. The secondary frame event is dispatched to the other devices, including the core.
	//6. Core output is dispatched.
	
	event::frame ev;
	
	ev.secondary=false;
	ev_dispatch(&ev); // #1
	ev_dispatch_chain(lock_xchg(&this->ev_head_thread, NULL)); // #2
	ev_dispatch_all(); // #2
	
	ev.secondary=true;
	ev.blockstate = event::bs_blocked;//this is a bit ugly, but I'd rather not split the function.
	ev_dispatch(&ev); // #3
	ev_dispatch_all(); // #4
	ev.blockstate = event::bs_resent;
	ev_dispatch(&ev); // #5
	
	ev_dispatch_all(); // #6
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
