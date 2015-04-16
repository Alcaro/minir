#include "minir.h"
#include <string.h>

class minir::devmgr_impl : public devmgr {
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

minir::devmgr* minir::devmgr::create() { return new devmgr_impl(); }
