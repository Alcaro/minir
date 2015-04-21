#include "minir.h"
#include <string.h>

namespace minir {
namespace {
class devmgr_impl : public devmgr {
public:

struct buttondat {
	device* dev;
	uint16_t id;
	//char padding[6];
};
inputmapper* mapper;
array<buttondat> buttons;
multiint<uint16_t> holdfire;


struct devinf_i {
	device* dev;
	array<string> inmap;
	//TODO
};
array<devinf_i> devices;

/*private*/ template<typename T> void add_device_core(device* dev, arrayview<T> inputs)
{
	size_t id = devices.len();
	dev_setup(dev, id);
	
	devinf_i& devdat = devices[id];
	devdat.dev = dev;
	devdat.inmap.resize(inputs.len());
	for (size_t i=0;i<inputs.len();i++)
	{
		devdat.inmap[i] = inputs[i];
	}
}
void add_device(device* dev, arrayview<string> inputs) { add_device_core(dev, inputs); }
void add_device(device* dev, arrayview<const char *> inputs) { add_device_core(dev, inputs); }

bool map_devices()
{
	//parse 'inmap'
	//
	//assign missing inputs
	//  while the last iteration did something:
	//    list all unused outputs from devices with all inputs satisfied
	//    for each unspecified input:
	//      if there is exactly one unused output of that type, use it
	//
	//discard unused devices:
	//  while the last iteration did something:
	//    find all devices that do not output to 'io_user' and whose outputs are not listened to
	//    toss them onto the garbage pile
	//    find all devices that do not have any attached input; 'io_user' counts as attached
	//    toss them onto the garbage pile
	//
	//find a suitable order for frame events
	//  mark all devices 'unordered'
	//  while there are unordered devices:
	//    take all unordered devices which do not take input events from unordered devices
	//    if there are no such devices: return error
	//    add them to the device array
	//    mark these devices ordered
	//
	//look for output loops
	//  same method as above, with the obvious changes
	//
	//check which devices need thread safety
	//  for (local device in frame_event.output.listeners):
	//    device.input_thread = main_thread
	//  
	//  local set<device_t> loop;
	//  loop.add(devices.select_where(this.input.io_frame || this.output.io_thread))
	//  
	//  while (local sender = loop.pop_any()):
	//    local sender_out_thread
	//    if sender.output.io_thread:
	//      sender_out_thread = sender
	//    else
	//      # if we're both taking frame events and non-main-thread input, we're going to get a conflict
	//      # the threader will send all our input to the main thread, so we'll mark our output as main thread too,
	//      # because that's where we're going to end up
	//      sender_out_thread = sender.input_thread
	//    for (local listener in sender.output.listeners):
	//      local prev_input_thread = listener.input_thread
	//      if listener.input_thread.eq_any(null, sender_out_thread):
	//        listener.input_thread = sender_out_thread
	//      else:
	//        listener.input_thread = main_thread
	//      if prev_input_thread != listener.input_thread && !listener.output.io_thread:
	//        loop.add(listener)
	//
	//  for (local device in devices):
	//    for (local sender in device.input.sources):
	//      local sender_out_thread = (sender.output.io_thread ? sender : sender.input_thread)
	//      if (sender_out_thread != device.input_thread):
	//        device = new device_threadwrap(device)
	//        break
	
	return false;
}


void frame()
{
	
}


devmgr_impl()
{
	
}

~devmgr_impl()
{
	
}

};
}

devmgr* devmgr::create() { return new devmgr_impl(); }
}
