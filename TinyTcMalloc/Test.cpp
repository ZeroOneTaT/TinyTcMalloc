#define _CRT_SECURE_NO_WARNINGS 1
#include "MemoryPool.h"
#include "TinyTcMalloc.h"

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode() :_val(0), _left(NULL), _right(NULL) {}
	TreeNode(int x) : _val(x), _left(nullptr), _right(nullptr) {}
};

// 测试定长内存池
void TestMemoryPool()
{
	// 申请释放的轮次
	const size_t Rounds = 5;
	// 每轮申请释放多少次
	const size_t N = 1000000;
	size_t s1 = clock();
	std::vector<TreeNode*> v1;
	v1.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}
	size_t e1 = clock();

	MemoryPool<TreeNode> TNPool;
	size_t s2 = clock();
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < 100000; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t e2 = clock();
	cout << "The Cost Time of New 5x1000000 iters : " << e1 - s1 << " ms" << endl;
	cout << "The Cost Time of MemoryPool 5x1000000 iters :" << e2 - s2 << " ms" << endl;
}


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

// 测试内存分配与释放接口
void TestMallocAndFree()
{
	void* ptr0 = TcMalloc(5);
	void* ptr1 = TcMalloc(8);
	void* ptr2 = TcMalloc(8);

	TcFree(ptr0);
	TcFree(ptr1);
	TcFree(ptr2);
}

// 测试大块内存申请释放
void TestBigMemory()
{
	void* ptr1 = TcMalloc(65 * 1024);
	TcFree(ptr1);

	//也有可能申请的是一块大于128页的内存
	void* ptr2 = TcMalloc(129 * 4 * 1024);
	TcFree(ptr2);
}


//
//int main()
//{
//	//TestMemoryPool();
//	//TestThreads();
//	TestSizeClass();
//	//TestMallocAndFree();
//	//TestBigMemory();
//	return 0;
//}