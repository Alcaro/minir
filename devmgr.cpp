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
	//list all device outputs
	//expected results:
	//one device, one output
	//  vgamepad
	//one device, multiple outputs, io_multi
	//  kb
	//  kb3
	//one device, multiple outputs, not io_multi
	//  dev.2
	//one device, multiple named outputs
	//  dev.named
	//  dev.alsonamed
	//multiple devices, one output each
	//  vgamepad2
	//multiple devices, one anonymous and one named output
	//  mouse2
	//  mouse2.wheel
	//multiple devices, two anonymous outputs each plus some named
	//  dev1.1
	//  dev3.2
	//  dev2.named
	//.left or similar can be appended to all of the above to access their components
	//to turn a joystick into arrow keys: joy.left(0.5) or joy.left(50%)
	// 1 (or 100%) is the leftmost possible value, 0 is middle
	// only [0..1] are allowed, where 0 doesn't fire if the stick is to the right
	//
	//there are also 'true' and 'false' buttons, though 'false' is useless since blank mappings are also false.
	//
	//TODO: figure out devices with multiple anon outputs of different kinds, and what to do with that
	// kb1, joy3, but it's gonna be tough.
	//
	//TODO: figure out what to do with analogs, they're not going to fit in 32 bits
	
	stringmap<size_t> sources;
	stringmap<size_t> multisources;
	
	sources["true"] = (size_t)-1; // special cases, given special treatment
	sources["false"] = (size_t)-1;
	
	{
		stringmap<size_t> n_sources; // extra scope here to throw out this one when it's done
		
		for (size_t i=0;i<devices.len();i++)
		{
			device* dev = devices[i].dev;
			
			//check if this one emits io_multi
			for (size_t j=0;dev->info->output[j];j++)
			{
				if (dev->info->output[j] == io_multi)
				{
					if (multisources.has(dev->info->name))
					{
						error(S"Cannot have multiple devices of type "+dev->info->name);
						return false;
					}
					multisources.set(dev->info->name, i);
					//continue execution, we want this in sources[] too
				}
			}
			
			size_t count = ++n_sources[dev->info->name];
			
			if (count==1)
			{
				sources[dev->info->name] = i;
			}
			else
			{
				size_t prev;
				if (count==2)
				{
					prev = sources[dev->info->name];
					sources[(S dev->info->name)+"1"] = prev;
					sources.remove(dev->info->name);
				}
				else prev = sources[(S dev->info->name)+"1"];
				
				//this is supposed to be a pointer (identity) comparison; devices with the same name must have the same devinfo
				//not 'return false' because this is an internal bug
				if (devices[prev].dev->info != dev->info) debug_abort();
				
				sources[(S dev->info->name) + string(count)] = i;
			}
		}
	}
	
	
	//parse 'inmap'
	
	
	//assign missing inputs
	//  while the last iteration did something:
	//    list all unused outputs from devices with all inputs satisfied
	//    for each unspecified input:
	//      if there is exactly one unused output of that type, use it
	//      if there is exactly one output of that type, use it
	//  if there is still any missing input, leave it unassigned; most weird inputs should be in 'inmap' already.
	
	
	//discard unused devices
	//  while the last iteration did something:
	//    find all devices that do not output to 'io_user' and whose outputs are not listened to
	//    toss them onto the garbage pile
	//    find all devices that do not have any attached input; 'io_user' is a valid input
	//    toss them onto the garbage pile
	
	
	//find a suitable order for frame events
	//  mark all devices 'unordered'
	//  while there are unordered devices:
	//    take all unordered devices which do not take input events from unordered devices
	//    if there are no such devices: return error
	//    add them to the device array
	//    mark these devices ordered
	
	
	//look for output loops
	//  same method as above, with the obvious changes
	
	
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
