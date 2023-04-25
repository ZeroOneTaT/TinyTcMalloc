#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <time.h>
#include <assert.h>
#include <mutex>
#include <unordered_map>
#include <algorithm>

using std::cout;
using std::endl;

#ifdef _WIN32
	#include <Windows.h>
#else
	// ......
#endif // _WIN32

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	//Linux
#endif

static const size_t MAX_BYTES = 256 << 10;		// Thread Cache中存储的最大内存大小 256 * 1024byte
static const size_t NFREELIST = 208;			// ThreadCache/CentralCache哈希桶的个数
static const size_t NPAGES = 128;				// PageCache中的最大页数，映射桶的最大数量 128 page = 1MB
static const size_t PAGE_SHIFT = 13;			// 申请的一页内存大小 8 * 1024kb = 2^13 byte

// 内存指针转换 4/32位 8/64位
static inline void*& NextObj(void* obj) 
{
	return *(void**)obj;
}

// 自定义内存申请函数
inline static void* SystemAlloc(size_t kPage)
{
	#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kPage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	#else
		//Linux下brk mmap等
	#endif // _WIN32

	//抛出异常
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// 自定义释放内存函数
inline static void SystemFree(void* ptr)
{
	#ifdef _WIN32
		VirtualFree(ptr, 0, MEM_RELEASE);
	#else
		//sbrk unmmap等
	#endif // _WIN32

}

// 自由链表，管理切分好的对象
class FreeList {
public:
	//将归还的内存块对象头插进自由链表
	void Push(void* obj)
	{
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}
	// 插入一段内存
	void PushRange(void* start, void* end, size_t size)
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += size;
	}
	// 将自由链表中的内存块头删出去
	void* Pop()
	{
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}
	// 删除一段内存
	void PopRange(void*& start, size_t n)
	{
		assert(n >= _size);
		start = _freeList;
		void* end = _freeList;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		_size -= n;
		NextObj(end) = nullptr;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}
	size_t& MaxSize()
	{
		return _maxSize;
	}
	size_t& Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;			//慢增长用于保住申请批次下限
	size_t _size = 0;				//链表长度
};


// 管理对齐和映射关系
class SizeClass {
public:
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐			freelist[0,16)			碎片率 (8-1)/8
	// [128+1,1024]				16byte对齐			freelist[16,72)			碎片率 (128+16-129)/(128+16)
	// [1024+1,8*1024]			128byte对齐			freelist[72,128)		碎片率 (1024+128-1025)/(1024+128)
	// [8*1024+1,64*1024]		1024byte对齐		freelist[128,184)		碎片率 (8*1024+1024-8*1024+1)/(8*1024+1024)
	// [64*1024+1,256*1024]		8*1024byte对齐		freelist[184,208)		碎片率 (64*1024+8*1024-64*1024+1)/(64*1024+8*1024)

	// 位运算将size对齐到>=size的alignNum的倍数
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return (bytes + alignNum - 1) & ~(alignNum - 1);
	}
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
			return _RoundUp(size, 8);
		else if (size <= 1024)
			return _RoundUp(size, 16);
		else if (size <= 8 * 1024)
			return _RoundUp(size, 128);
		else if (size <= 64 * 1024)
			return _RoundUp(size, 1024);
		else if (size <= 256 * 1024)
			return _RoundUp(size, 8 * 1024);
		else
			return _RoundUp(size, 1 << PAGE_SHIFT);
	}

	// 将 size 映射到桶链的下标：
	// 计算方法是将 size 向上对齐到最接近它的大于等于它的 2^align_shift(即alignNum) 的"倍数"，然后倍数再减去 1。
	// 这个函数的作用和 _RoundUp 函数类似，但是它返回的是下标而不是对齐后的值。
	// 比如size = 11映射下标到(2 - 1 = 1) 
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	// 计算映射到哪个自由桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		static int group_array[4] = { 16, 56 + 16, 56 + 56 + 16, 56 + 56 + 56 + 16 };
		if (bytes <= 128)
			return _Index(bytes, 3);					// 2^3
		else if (bytes <= 1024)
			return _Index(bytes, 4) + group_array[0];	// 2^4
		else if (bytes <= 8 * 1024)
			return _Index(bytes, 7) + group_array[1];	// 2^7
		else if (bytes <= 64 * 1024)
			return _Index(bytes, 10) + group_array[2];	// 2^10
		else if (bytes <= 256 * 1024)
			return _Index(bytes, 13) + group_array[3];	// 2^3
		else
			assert(false);

		return -1;
	}

	// 内存不够，向CentralCache申请
	// 计算ThreadCache一次从中心缓存CentralCache获取多少个小对象，总的大小就是MAX_BYTES = 256KB
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512], 一次批量移动多少个对象的上限值（慢启动）
		// 根据批次计算获取的数量会在[1,32768]，范围过大，
		// 因此控制获取的对象数量范围在区间[2, 512],较为合理
		// 小对象获取的批次多，大对象获取的批次少
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;
		if (num > 512)
			num = 512;
		return num;
	}

	// 计算中心缓存CentralCache一次向PageCache获取多少页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		// 计算一次从中心缓存获取的对象个数num
		size_t num = NumMoveSize(size);
		// 单个对象大小与对象个数相乘,获得一次需要向PageCache申请的内存大小
		size_t n_page = num * size;

		n_page >>= PAGE_SHIFT;
		if (0 == n_page)
			n_page = 1;

		return n_page;
	}
private:
	//
};


// CentralCache管理多个连续页大块内存跨度结构
struct Span
{
	PAGE_ID _pageID = 0;	// 大内存起始页id
	size_t _num = 0;		// 页数量

	Span* _prev = nullptr;
	Span* _next = nullptr;

	size_t _objSize = 0;	// Span切分小块内存大小
	bool _isUsed = false;	// Span是否被使用标志
	size_t _usedCount = 0;	// Span已分配给ThreadCache的内存数量
	void* _freeList = nullptr;
};

// Span带头双向链表结构
class SpanList
{
public:
	SpanList()
	{
		_head = new Span();
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	// 在链表指定位置插入新的内存块
	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		pos->_prev = newSpan;
	}
	
	// 从链表中删除指定内存块
	void Erase(Span* pos)
	{
		assert(pos);
		// 指向链表头为非法位置
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	// 头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// 头删，并返回删除的结点指针；并不是真正的删除，而是把关系解除掉
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	// 桶锁上锁
	void lock() { _mtx.lock(); }
	// 桶锁解锁
	void unlock() { _mtx.unlock(); }

private:
	Span* _head;		// 链表的头指针

	std::mutex _mtx;	// 桶锁,保持线程安全
};