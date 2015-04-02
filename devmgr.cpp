#include "minir.h"
#include <string.h>

class devmgr::impl : public devmgr {
public:

struct devicedat {
	device* obj;
	uint32_t events[2];//[0]=primary, [1]=secondary (index with event->secondary)
};

//[0] is the core, [1] is the primary device
struct devicedat * devices;
size_t numdevices;


fifo<event> events;

smutex t_mut;
fifo<event> t_events;


struct buttondat {
	device* dev;
	unsigned int id;
	uint8_t type;
	//char padding[3];
};
inputmapper* mapper;
array<buttondat> buttons;
multiint<uint16_t> holdfire;

uint8_t buttonblock;


void ev_append(const event& ev)
{
	events.push(ev);
}

void ev_append_async(const event& ev)
{
	synchronized(t_mut) {
		t_events.push(ev);
	}
}

/*private*/ void ev_do_dispatch(const event& ev, device* dev)
{
	switch (ev.type)
	{
#define HANDLE_EVENT(name) \
	case event::ty_##name: \
		dev->ev_##name(ev); \
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

/*private*/ void ev_dispatch_to(const event& ev, size_t index)
{
	static const uint16_t subscriber_bits[]={
		0,
		e_frame, e_savestate, e_savestate,
		e_video, e_audio,
		e_keyboard, e_mouse, e_mouse, e_gamepad,
	};
	
	if (ev.source != this->devices[index].obj &&
	    subscriber_bits[ev.type] & this->devices[index].events[ev.secondary])
	{
		//no need to null check obj; null objs ask for no events, so they won't get here
		ev_do_dispatch(ev, this->devices[index].obj);
	}
}

/*private*/ void ev_dispatch(const event& ev)
{
	if (ev.blockstate != event::bs_resent) ev_dispatch_to(ev, 1);
	
	if (ev.type == event::ty_keyboard) // TODO: mousemove, mousebutton, gamepad
	{
		buttonblock = ev.blockstate;
		mapper->event(inputmapper::dev_kb, ev.keyboard.deviceid, ev.keyboard.libretrocode, ev.keyboard.scancode, ev.keyboard.down);
	}
	
	if (ev.blockstate == event::bs_blocked) return;
	for (size_t i=2;i<this->numdevices;i++)
	{
		ev_dispatch_to(ev, i);
	}
	ev_dispatch_to(ev, 0);
}

/*private*/ void ev_dispatch_all()
{
	while (!events.empty())
	{
		event ev = events.pop_checked();
		ev_dispatch(ev);
	}
}

/*private*/ void ev_button(unsigned int id, inputmapper::triggertype events)
{
	buttondat& button = buttons[id];
	event ev(event::ty_button);
	ev.button.id = buttons[id].id;
	
	//there's so many possibilities here; three event types, and three event flags, each of which can be set individually
	if (button.type == device::btn_event)
	{
		if (events&inputmapper::tr_primary) ev.button.down = true;
		else return;
	}
	else
	{
		if ((events&(inputmapper::tr_press|inputmapper::tr_release)) == 0) return;
		ev.button.down = (events&inputmapper::tr_press);
		if (button.type == device::btn_hold)
		{
			if (ev.button.down)
			{
				holdfire.add(id);
				return;//btn_hold fires each frame, but not as a direct response to events
			}
			else holdfire.remove(id);
		}
	}
	
	button.dev->ev_button(ev);
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
	int newid = mapper->register_button(desc);
	if (newid < 0) return false;
	
	buttondat& button = buttons[newid];
	button.dev = target;
	button.id = id;
	button.type = when;
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
	//2. Asynchronously submitted events are processed.
	//3. All events created by the above are processed.
	//4. Button hold events are dispatched.
	//5. All events created by that are processed.
	//6. The secondary frame is sent to the primary device.
	//7. Events created by that are processed.
	//8. The secondary frame event is dispatched to the other devices, including the core.
	//9. Core output is dispatched.
	
	event ev(event::ty_frame);
	
	ev.secondary=false;
	ev_dispatch(ev); // #1
	
	// #2
	t_mut.lock();
	while (!t_events.empty())
	{
		event ev = t_events.pop_checked();
		t_mut.unlock();
		ev_dispatch(ev);
		t_mut.lock();
	}
	t_mut.unlock();
	
	ev_dispatch_all(); // #3
	
	// #4
	uint16_t numheld;
	uint16_t* held = holdfire.get(numheld);
	if (numheld)
	{
		event ev(event::ty_button);
		ev.button.down = true;
		for (uint16_t i=0;i<numheld;i++)
		{
			ev.button.id = buttons[held[i]].id;
			buttons[held[i]].dev->ev_button(ev);
		}
	}
	
	ev_dispatch_all(); // #5
	
	ev.secondary=true;
	ev.blockstate = event::bs_blocked;//this is a bit ugly, but I'd rather not split the function.
	ev_dispatch(ev); // #6
	ev_dispatch_all(); // #7
	ev.blockstate = event::bs_resent;
	ev_dispatch(ev); // #8
	
	ev_dispatch_all(); // #9
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
