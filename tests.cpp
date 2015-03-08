#ifdef SELFTEST
//cls & g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp -g & gdb a.exe
//      g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp -g && gdb ./a.out
#include "os.h"
#include "containers.h"

static void assert(bool cond)
{
	if (!cond) debug_abort();
}

#ifdef OS_POSIX
#include <valgrind/memcheck.h>
#else
#define VALGRIND_DO_LEAK_CHECK
#endif

static void test_multiint()
{
	multiint<uint16_t> obj;
	uint16_t* ptr;
	uint16_t num;
	
	ptr = obj.get(num);
	assert(num == 0);
	
	obj.add(0x123);
	
	ptr = obj.get(num);
	assert(num == 1);
	assert(ptr[0] == 0x123);
	
	obj.add(0x123);
	ptr = obj.get(num);
	assert(num == 1);
	assert(ptr[0] == 0x123);
	
	obj.add(123);
	ptr = obj.get(num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+123);
	
	obj.add(1234);
	ptr = obj.get(num);
	assert(num == 3);
	assert(ptr[0]+ptr[1]+ptr[2] == 0x123+123+1234);
	
	obj.remove(123);
	ptr = obj.get(num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+1234);
	
	obj.remove(123);
	ptr = obj.get(num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+1234);
	
	obj.remove(0x123);
	ptr = obj.get(num);
	assert(num == 1);
	assert(ptr[0] == 1234);
	
	obj.remove(1234);
	ptr = obj.get(num);
	assert(num == 0);
	
	obj.add(123);
	obj.add(1234);
	obj.remove(123);
	ptr = obj.get(num);
	assert(num == 1);
	assert(ptr[0]==1234);
	obj.remove(1234);
	
	obj.add(123);
	obj.add(1234);
	obj.remove(1234);
	ptr = obj.get(num);
	assert(num == 1);
	assert(ptr[0]==123);
	obj.remove(123);
	
	obj.add(1);
	obj.add(12);
	obj.add(123);
	obj.add(1234);
	obj.add(12345);
	obj.remove(123);
	ptr = obj.get(num);
	assert(num == 4);
	assert(ptr[0]+ptr[1]+ptr[2]+ptr[3] == 1+12+1234+12345);
	obj.remove(1);
	obj.remove(12);
	obj.remove(123);
	obj.remove(1234);
	obj.remove(12345);
	
	obj.add(1);
	obj.add(12);
	obj.add(123);
	obj.add(1234);
	obj.add(12345);
	obj.remove(12345);
	ptr = obj.get(num);
	assert(num == 4);
	assert(ptr[0]+ptr[1]+ptr[2]+ptr[3] == 1+12+123+1234);
	
	//leave it populated - leak check
	//VALGRIND_DO_LEAK_CHECK;
}

int main()
{
	test_multiint();
}
#endif
