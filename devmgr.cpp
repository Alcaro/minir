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
	array<string> sourcenames_rev;
	
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
				sourcenames_rev[i] = string(dev->info->name);
			}
			else
			{
				uint16_t prev;
				if (count==2)
				{
					prev = sourcenames[namelower];
					sourcenames[namelower+"1"] = prev;
					sourcenames.remove(namelower);
					
					sourcenames_rev[prev] = string(dev->info->name)+"1";
				}
				else prev = sourcenames[namelower+"1"];
				
				//this is supposed to be a pointer (identity) comparison; devices with the same name must have the same devinfo
				//not 'return false' because this is an internal bug
				if (devices[prev].dev->info != dev->info) debug_abort();
				
				sourcenames[namelower + string(count)] = i;
				sourcenames_rev[i] = string(dev->info->name) + string(count);
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
	
	//for dependencies[index to 'devices'], each listed device is used as an input to the given device
	//_in and _out contain only those devices that listen to input or output events
	//_in and _out are used to detect disallowed loops and to order the frame event
	//the full one is used for thread safety and garbage collection
	array<multiint<uint16_t> > dependencies;
	array<multiint<uint16_t> > dependencies_in;
	array<multiint<uint16_t> > dependencies_out;
	
	//mappings[index to 'devices'][index to 'inmap'] = output ID; 12bit device, 8bit output ID, 12bit unused (zero)
	//unassigned and button/event is 0xFFFFFFFF; buttons are complex and treated differently
	//array<array<uint32_t> > mappings;
	
	{
		for (size_t i=0;i<devices.len();i++)
		{
			devinf_i* inf = &devices[i];
			for (size_t j=0;inf->dev->info->input[j];j++)
			{
				enum iotype type = inf->dev->info->input[j];
				enum iogroup group = iog_find(type);
				
				if (inf->inmap[j])
				{
					char* desc = inf->inmap[j];
					char * descend = desc;
					while (*descend)
					{
						char * nameend = NULL;
						while (*descend!='\0' && *descend!='+' && *descend!=',')
						{
							if (*descend=='.') nameend=descend;
							descend++;
						}
						char descsep = *descend;
						if (descsep!='\0' && type!=io_button && type!=io_event)
						{
							error(S"Device "+inf->dev->info->name+" has an invalid input descriptor");
							break;
						}
						*descend = '\0';
						
						char * name = desc;
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
							error(S"Device "+sourcenames_rev[i]+" refers to device "+name+", which doesn't exist");
						}
						
						if (nameend) *nameend = '.';
						
						if (device)
						{
							dependencies[i].add(*device);
							if (group==iog_in) dependencies_in[i].add(*device);
							if (group==iog_out) dependencies_out[i].add(*device);
						}
						
						*descend = descsep;
						desc = descend;
						if (*desc=='+') desc++;
						else if (*desc==',')
						{
							desc++;
							while (*desc==' ') desc++;
						}
						descend=desc;
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
	array<bool> used;
	{
		
	}
	
	//find a suitable order for frame events
	//  mark all devices 'unordered'
	//  while there are unordered devices:
	//    take all unordered devices which do not take input events from unordered devices
	//    if there are no such devices: return error
	//    add them to the device array
	//    mark these devices ordered
	//also do the same check for the output events
	array<uint16_t> order;
	{
		enum pass_t { p_input, p_output, p_end };
		for (int pass=0;pass<p_end;pass++)
		{
			//dep[devidA].contains(devidB) = (B before A)
			array<multiint<uint16_t> >& dependencies = (pass==p_input ? dependencies_in : dependencies_out);
			
			multiint<uint16_t> unordered;
			for (size_t i=0;i<devices.len();i++)
			{
				unordered.add_uniq(i);
			}
			
			while (true)
			{
				bool did_anything = false;
				
				for (int i=0;i<unordered.count();i++)
				{
					bool free = true;
					multiint<uint16_t>& mydeps = dependencies[unordered[i]];
					for (int j=0;j<mydeps.count();j++)
					{
						if (unordered.contains(mydeps[j])) { free=false; break; }
					}
					if (free)
					{
						unordered.remove(unordered[i]);
						if (pass==p_input) order.append(i);
						i--;
						did_anything=true;
					}
				}
				if (unordered.count()==0) break;
				
				if (!did_anything)
				{
					string errmsg;
					errmsg += (pass==p_input ? "Input" : "Output");
					errmsg += " loop between these devices:";
					
					uint16_t id1 = unordered[0];
					uint16_t id2 = unordered[0];
					
					//no way to create nested functions without this
					class next {
					public:
						static uint16_t get(array<multiint<uint16_t> >& dependencies, multiint<uint16_t>& unordered, uint16_t id)
						{
							uint16_t i=0;
							while (!unordered.contains(dependencies[id][i])) i++;
							return dependencies[id][i];
						}
					};
					
#define next(id) next::get(dependencies, unordered, id)
					do {
						id1 = next(id1);
						id2 = next(next(id2));
					} while (id1!=id2);
					
					do {
						id1 = next(id1);
						errmsg += " ";
						errmsg += sourcenames_rev[id1];
					} while (id1!=id2);
#undef next
					
					error(errmsg);
					return false;
				}
			}
		}
	}
	
	
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
