#include "os.h"

#ifdef _WIN32
#undef bind
#include <windows.h>
#define bind bind_func

void debug_break()
{
	if (IsDebuggerPresent()) DebugBreak();
}

void debug_abort()
{
	DebugBreak();
	FatalExit(1);
}
#endif

#ifdef __unix__
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

//method from https://src.chromium.org/svn/trunk/src/base/debug/debugger_posix.cc
static bool has_debugger()
{
	char buf[4096];
	int fd = open("/proc/self/status", O_RDONLY);
	if (!fd) return false;
	
	ssize_t bytes = read(fd, buf, sizeof(buf)-1);
	close(fd);
	
	if (bytes < 0) return false;
	buf[bytes] = '\0';
	
	const char * tracer = strstr(buf, "TracerPid:\t");
	if (!tracer) return false;
	tracer += strlen("TracerPid:\t");
	
	return (*tracer != '0');
}

void debug_break()
{
	if (has_debugger()) raise(SIGTRAP);
}

void debug_abort()
{
	raise(SIGTRAP);
	abort();
}
#endif
