#include "minir.h"
#include <string.h>

namespace minir {
namespace {
class devmgr_impl : public devmgr {
public:

struct buttondat {
	device* dev;
	uint16_t id;
	uint8_t type;
	//char padding[5];
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
bool dev_mapped;


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
	
	dev_mapped = false;
}

void add_device(device* dev, arrayview<string> inputs) { add_device_core(dev, inputs); }
void add_device(device* dev, arrayview<const char *> inputs) { add_device_core(dev, inputs); }


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
