#pragma once
/*
	ThreadCache封装
	为每一个线程提供一个独立的ThreadCache
*/
#include "Common.h"

class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// 内存不足，从CentralCache获取内存对象
	void* FetchFromCentralCache(size_t index, size_t size);

	// 释放内存时，如自由链表过长，回收内存到CentralCache
	void FreeToCentralCache(FreeList& list, size_t size);
private:
	// 哈希桶，每个桶中挂在对应大小的自由链表对象
	FreeList _freeLists[NFREELIST];
};

// pTLSThreadCache是一个指向ThreadCache对象的指针，每个线程都有一个独立的pTLSThreadCache
// 可以使线程在向thread cache申请内存对象的时候实现无锁访问
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;