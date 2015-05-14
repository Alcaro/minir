#include "minir.h"
#include <string.h>
#include <ctype.h>
#include "hashmap.h"
#include "multiint.h"

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
	//to turn a joystick into arrow keys: joy.left, hardcoded to trigger if the stick is on 50% of max distance
	// (may be extended later, if anyone cares)
	//
	//there are also 'true' and 'false' buttons, though 'false' is useless since blank mappings are also false.
	//
	//TODO: figure out devices with multiple anon outputs of different kinds, and what to do with that
	// dev.kb1, dev.joy3, it's gonna be messy.
	
	hashmap<string,uint16_t> sourcenames;
	hashmap<string,uint16_t> multisourcenames;
	
	sourcenames["true"] = (uint16_t)-1; // special cases, given special treatment
	sourcenames["false"] = (uint16_t)-1;
	
	{
		hashmap<string,uint16_t> n_sourcenames; // extra scope here to throw out this one when it's done
		
		for (uint16_t i=0;i<devices.len();i++)
		{
			device* dev = devices[i].dev;
			
			string namelower = string(dev->info->name).lower();
			
			//check if this one emits io_multi
			for (uint16_t j=0;dev->info->output[j];j++)
			{
				if (dev->info->output[j] == io_multi)
				{
					if (multisourcenames.has(namelower))
					{
						error(S"Cannot have multiple devices of type "+dev->info->name);
						return false;
					}
					multisourcenames.set(namelower, i);
					//continue execution, we want this in sources[] too
				}
			}
			
			uint16_t count = ++n_sourcenames[namelower];
			
			if (count==1)
			{
				sourcenames[namelower] = i;
			}
			else
			{
				uint16_t prev;
				if (count==2)
				{
					prev = sourcenames[namelower];
					sourcenames[namelower+"1"] = prev;
					sourcenames.remove(namelower);
				}
				else prev = sourcenames[namelower+"1"];
				
				//this is supposed to be a pointer (identity) comparison; devices with the same name must have the same devinfo
				//not 'return false' because this is an internal bug
				if (devices[prev].dev->info != dev->info) debug_abort();
				
				sourcenames[namelower + string(count)] = i;
			}
		}
	}
	
	
	//parse 'inmap'
	
	//possible input mappings:
	//kb.x
	//kb.x+kb.y
	//kb.x, kb.y
	//kb.f1
	//kb.lshift+kbf1, kb.rshift+kb.f1
	//vgamepad.lanalog.left
	
	//dependencies[index to 'devices'] contains all devices that must be before this one, including button/event
	array<multiint<uint16_t> > dependencies;
	
	//mappings[index to 'devices'][index to 'inmap'] = output ID; 12bit device, 8bit output ID, 12bit unused (zero)
	//unassigned and button/event is 0xFFFFFFFF; buttons are complex and treated differently
	//array<array<uint32_t> > mappings;
	
	{
		for (size_t i=0;i<devices.len();i++)
		{
			devinf_i* inf = &devices[i];
			for (size_t j=0;inf->dev->info->input[j];j++)
			{
				if (inf->inmap[j])
				{
					char* name = inf->inmap[j];
					char * nameend = strchr(name, '.');
					if (nameend) *nameend = '\0';
					
					uint16_t* device = sourcenames.get_ptr(name);
					if (!device)
					{
						char * namenul = nameend ? nameend : strchr(name, '\0');
						if (namenul != name)
						{
							while (isdigit(namenul[-1])) namenul--;
							
							char digit = *namenul;
							*namenul = '\0';
							device = multisourcenames.get_ptr(name);
							*namenul = digit;
						}
					}
					
					if (!device)
					{
						error(S"Device "+inf->dev->info->name+" refers to device "+name+", which doesn't exist");
					}
					
					if (nameend) *nameend = '.';
					
					if (device)
					{
						dependencies[i].add(*device);
					}
				}
			}
		}
	}
	
	
	//assign missing inputs
	//  while the last iteration did something:
	//    list all unused outputs from devices with all inputs satisfied
	//    for each unspecified input:
	//      if there is exactly one unused output of that type, use it
	//      if there is exactly one output of that type, use it
	//  if there is still any missing input, leave it unassigned; most weird inputs should be in 'inmap' already.
	
	//event/button can be mapped to complex things, do I need to put them separately somehow?
	//or perhaps it's reasonably easy - event and button should not be assigned because there's a thousand possible choices for them
	
	//could the input assigner possibly generate loops or other nasty conditions? Not likely, unassigned inputs are rare.
	//could I combine the frame-order assigner with the input assigner, and only assign outputs backwards? Could that give weird order-based randomness?
	//can I deprioritize devices with unassigned inputs that can't be satisfied? Can I only assign inputs if the device has exactly one input?
	//perhaps I should not autoassign at all, and let the GUI do it? That should be the most logical method, GUI can know whether it's autoconfigurable.
	
	
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
	
	{
		
	}
	
	
	//look for output loops
	//  same method as above, with the obvious changes
	
	
	//check which devices need thread safety
	//TODO: given that the frame orderer comes first, this one should be possible to simplify
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
