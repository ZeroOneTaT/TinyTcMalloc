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

static const size_t MAX_BYTE = 256 << 10;		// Thread Cache中存储的最大内存大小 256 * 1024byte
static const size_t NFREELIST = 208;			// ThreadCache/CentralCache哈希桶的个数
static const size_t NPAGES = 128;				// PageCache中的最大页数，映射桶的最大数量 128 page = 1MB
static const size_t PAGE_SHIFT = 13;			// 申请的一页内存大小 8 * 1024kb = 2^13 byte

// 内存指针转换 4/32位 8/32位
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