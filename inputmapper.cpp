#include "minir.h"

namespace {
class devmgr_inputmapper_impl : public devmgr::inputmapper {
	//function<void(size_t id, bool down)> callback;
	
	//each button maps to an uint32 which uniquely describes it
	//format:
	//ttttxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	//tttt=type
	// 0000=keyboard
	// others=TODO
	//x=varies depending on the device
	//
	//for keyboard:
	//0000---- -------- kkkkksnn nnnnnnnn
	//-=unused, always 0
	//k=keyboard ID, 1-31, or 0 for any; if more than 31, loops back to 16
	//s=is scancode flag
	//n=if s is set, scancode, otherwise libretro code
	//
	//00000000 00000000 00000000 00000000 (RETROK_NONE on any keyboard) is impossible and may be used as terminator
	
	size_t register_group(size_t len)
	{
		return 0;
	}
	
	bool register_button(size_t id, const char * desc)
	{
		return false;
	}
	
	//enum dev_t {
	//	dev_kb,
	//	dev_mouse,
	//	dev_gamepad,
	//};
	//enum { mb_left, mb_right, mb_middle, mb_x4, mb_x5 };
	//type is an entry in the dev_ enum.
	//device is which item of this type is relevant. If you have two keyboards, pressing A on both
	// would give different values here.
	//button is the 'common' ID for that device.
	// For keyboard, it's a RETROK_*. For mouse, it's the mb_* enum. For gamepads, [TODO]
	//scancode is a hardware-dependent unique ID for that key. If a keyboard has two As, they will
	// have different scancodes. If a key that doesn't map to any RETROK (Mute, for example), the common
	// ID will be some NULL value (RETROK_NONE, for example), and scancode will be something valid.
	//down is the new state of the button. Duplicate events are fine and will be ignored.
	void event(dev_t type, unsigned int device, unsigned int button, unsigned int scancode, bool down)
	{
		
	}
	
	void reset(dev_t type)
	{
		
	}
	
	~devmgr_inputmapper_impl()
	{
		
	}
};
}

devmgr::inputmapper* devmgr::inputmapper::create()
{
	return new devmgr_inputmapper_impl;
}
