#define _CRT_SECURE_NO_WARNINGS 1
#include "MemoryPool.h"
#include "TinyTcMalloc.h"

void func1()
{
	for (size_t i = 0; i < 10; ++i)
	{
		TcMalloc(17);
	}
}

void func2()
{
	for (size_t i = 0; i < 20; ++i)
	{
		TcMalloc(5);
	}
}

// 测试线程
void TestThreads()
{
	std::thread t1(func1);
	std::thread t2(func2);


	t1.join();
	t2.join();
}

// 测试对齐函数
void TestSizeClass()
{
	/*cout << SizeClass::Index(1035) << endl;
	cout << SizeClass::Index(1025) << endl;
	cout << SizeClass::Index(1024) << endl;*/
	cout << SizeClass::Index(7) << endl;
	cout << SizeClass::Index(8) << endl;
	cout << SizeClass::Index(28) << endl;
}

//// 测试内存分配与释放接口
//void TestMallocAndFree()
//{
//	void* ptr0 = TcMalloc(5);
//	void* ptr1 = TcMalloc(8);
//	void* ptr2 = TcMalloc(8);
//
//	TcFree(ptr0);
//	TcFree(ptr1);
//	TcFree(ptr2);
//}
//
//// 测试大块内存申请释放
//void TestBigMemory()
//{
//	void* ptr1 = TcMalloc(65 * 1024);
//	TcFree(ptr1);
//
//	//也有可能申请的是一块大于128页的内存
//	void* ptr2 = TcMalloc(129 * 4 * 1024);
//	TcFree(ptr2);
//}

int main()
{
	//TestObjectPool();
	//TestThreads();
	TestSizeClass();
	//TestMallocAndFree();
	//TestBigMemory();
	return 0;
}