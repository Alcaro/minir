#include "io.h"

#ifdef INPUTCUR_W32HOOK
namespace {
class inputcursor_w32hook : public inputcursor {
public:
	bool construct(uintptr_t windowhandle) { return false; }
	void refresh() {}
	void poll() {}
	~inputcursor_w32hook(){}
};

inputcursor* inputcursor_create_w32hook(uintptr_t windowhandle)
{
	inputcursor_w32hook* ret=new inputcursor_w32hook;
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

};

const inputcursor::driver inputcursor::driver_w32hook={ "WindowsHook", inputcursor_create_w32hook, 0 };
#endif
