#ifdef SELFTEST
//    cls & g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp -g & gdb a.exe
//rm a.out; g++ -DSELFTEST -DNOMAIN tests.cpp debug.cpp memory.cpp thread-linux.cpp -pthread -g && valgrind ./a.out
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
	//leaving 1, 12, 123, 1234 there
	
	for (int i=0;i<100;i++)
	{
		obj.add(i);
	}
	ptr = obj.get(num);
	assert(num==102); // 0..99, 123, 1234
	int sum=0;
	for (int i=0;i<num;i++) sum+=ptr[i];
	assert(sum==6307); // sum(0..99)=4950, +123+1234 =6307
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


//stupid C++ why can't I use statics as template arguments
/*static*/ void test_hashset_seen(bool* flags, uint32_t& n)
{
	for (uint32_t i=0;i<100;i++)
	{
		if (int_shuffle(i)==n)
		{
			assert(!flags[i]);
			flags[i]=true;
			return;
		}
	}
	assert(false);
}

static void test_hashset()
{
	{
		hashset<uint32_t> set;
		for (uint32_t i=0;i<100;i++)
		{
			for (uint32_t j=0;j<100;j++)
			{
				uint32_t val = int_shuffle(j);
				if (j<i) assert(set.has(val));
				else assert(!set.has(val));
			}
			uint32_t val = int_shuffle(i);
			set.add(val);
		}
		
		bool seen[100];
		for (int i=0;i<100;i++) seen[i]=false;
		set.each(bind_ptr(test_hashset_seen, seen));
		for (int i=0;i<100;i++) assert(seen[i]);
	}
	
	{
		hashset<void*> test_ptr; // just to test that this compiles cleanly
	}
	
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


namespace test_threads {
mutex2 mut;
int iter;

multievent ev;

void test1(void* idptr)
{
	int id = (int)(uintptr_t)idptr;
	//test light contention
	//expected duration: 1 second
	for (int i=0;i<1000;i++)
	{
		mut.lock();
		iter++;
		mut.unlock();
		thread_sleep(1000);
	}
	
	ev.signal();
}

void test2(void* idptr)
{
	int id = (int)(uintptr_t)idptr;
	//test heavy contention
	//expected duration: 4 seconds
	//due to the heavy contention on this mutex, there will be many sleeps and wakeups; 'time' may return raised sys numbers for this
	for (int i=0;i<1000;i++)
	{
		mut.lock();
		iter++;
		thread_sleep(1000);
		mut.unlock();
	}
	
	ev.signal();
}

void test3(void* idptr)
{
	int id = (int)(uintptr_t)idptr;
	//test heavy lock/unlock
	//expected duration: instant
	for (int i=0;i<10000;i++)
	{
		mut.lock();
		iter++;
		mut.unlock();
	}
	
	ev.signal();
}

static void main()
{
	iter=0;
	for (int i=0;i<4;i++)
	{
		thread_create(bind_ptr(test1, (void*)(uintptr_t)i));
	}
	for (int i=0;i<4;i++) ev.wait();
	assert(iter==4000);
	
	iter=0;
	for (int i=0;i<4;i++)
	{
		thread_create(bind_ptr(test2, (void*)(uintptr_t)i));
	}
	for (int i=0;i<4;i++) ev.wait();
	assert(iter==4000);
	
	iter=0;
	for (int i=0;i<4;i++)
	{
		thread_create(bind_ptr(test3, (void*)(uintptr_t)i));
	}
	for (int i=0;i<4;i++) ev.wait();
	assert(iter==40000);
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
	//test_threads::main(); // warning: takes a while.
}
#endif
