#include "io.h"

#ifdef INPUTCUR_WIN32
namespace {
class inputcursor_w32msg : public inputcursor {
public:
	bool construct(uintptr_t windowhandle) { return false; }
	void refresh() {}
	void poll() {}
	~inputcursor_w32msg(){}
};

inputcursor* inputcursor_create_w32msg(uintptr_t windowhandle)
{
	inputcursor_w32msg* ret=new inputcursor_w32msg;
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

};

const inputcursor::driver inputcursor::driver_w32msg={ "Win32", inputcursor_create_w32msg, 0 };
#endif
