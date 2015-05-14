#ifdef SELFTEST
//cls & g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp -g & gdb a.exe
//      g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp -g && gdb ./a.out
#include "os.h"
#include "containers.h"
#include "endian.h"

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

static void test_sort()
{
	{
		int items[4] = { 1,2,3,4 };
		sort(items, 4);
		assert(items[0]==1);
		assert(items[1]==2);
		assert(items[2]==3);
		assert(items[3]==4);
	}
	
	{
		int items[4] = { 1,3,4,2 };
		sort(items, 4);
		assert(items[0]==1);
		assert(items[1]==2);
		assert(items[2]==3);
		assert(items[3]==4);
	}
	
	{
		int items[4] = { 4,3,2,1 };
		sort(items, 4);
		assert(items[0]==1);
		assert(items[1]==2);
		assert(items[2]==3);
		assert(items[3]==4);
	}
}

static void test_fifo()
{
	{
		fifo<int> n;
		for (int i=0;i<4;i++)
		{
			assert(n.count() == 0);
			for (int i=1;i<=4;i++) n.push(i);
			assert(n.count() == 4);
			for (int i=1;i<=4;i++) assert(n.pop()==i);
		}
	}
	
	{
		fifo<int> n;
		assert(n.count() == 0);
		for (int i=1;i<=8;i++) n.push(i);
		assert(n.count() == 8);
		for (int i=1;i<=8;i++) assert(n.pop()==i);
	}
	
	{
		fifo<int> n;
		assert(n.count() == 0);
		for (int i=1;i<=12;i++) n.push(i);
		assert(n.count() == 12);
		for (int i=1;i<=12;i++) assert(n.pop()==i);
	}
	
	{
		fifo<int> n;
		assert(n.count() == 0);
		for (int i=1;i<=16;i++) n.push(i);
		assert(n.count() == 16);
		for (int i=1;i<=16;i++) assert(n.pop()==i);
	}
	
	{
		fifo<int> n;
		for (int i=1;i<=4;i++) n.push(i);
		for (int i=1;i<=4;i++) assert(n.pop()==i);
		for (int i=1;i<=15;i++) n.push(i);
		for (int i=1;i<=15;i++) assert(n.pop()==i);
	}
	
	{
		fifo<int> n;
		for (int i=1;i<=7;i++) n.push(i);
		for (int i=1;i<=7;i++) assert(n.pop()==i);
		for (int i=1;i<=15;i++) n.push(i);
		for (int i=1;i<=15;i++) assert(n.pop()==i);
	}
}

static void test_endian()
{
	bigend<uint32_t> t;
	uint8_t* p = (uint8_t*)&t;
	
	t = 0x01020304;
	assert(p[0]==0x01);
	assert(p[1]==0x02);
	assert(p[2]==0x03);
	assert(p[3]==0x04);
	
	t = 0x04030201;
	assert(p[0]==0x04);
	assert(p[1]==0x03);
	assert(p[2]==0x02);
	assert(p[3]==0x01);
	
	t += 0x02040608;
	assert(p[0]==0x06);
	assert(p[1]==0x07);
	assert(p[2]==0x08);
	assert(p[3]==0x09);
}

static void test_hashset()
{
	//this is far from comprehensive, it's just some really basic sanity checks
	{
		hashset<string> set;
		set.add("abc");
		assert(set.has("abc"));
		assert(!set.has("abcd"));
	}
	
	{
		hashmap<string,string> map;
		map.set("abc", "123");
		assert(map.get("abc")=="123");
	}
	
	hashset<void*> q;
}

static void test_bitarray()
{
	{
		array<bool> a;
		for (int i=0;i<=7;i++) assert(!a[i]);
		a[7]=true;
		for (int i=0;i<=6;i++) assert(!a[i]);
		assert(a[7]);
		for (int i=8;i<=15;i++) assert(!a[i]);
		
		for (int i=0;i<=7;i++) a[i]=true;
		a[15]=true;
		for (int i=0;i<=7;i++) assert(a[i]);
		for (int i=8;i<=14;i++) assert(!a[i]);
		assert(a[15]);
	}
}

int main()
{
	//test_multiint();
	//test_sort();
	//test_fifo();
	//test_endian();
	//test_hashset();
	//test_bitarray();
}
#endif
