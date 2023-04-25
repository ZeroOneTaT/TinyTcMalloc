#pragma once
/*
	外部申请内存、释放内存函数封装
*/

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "MemoryPool.h"

// 线程共享TreadCache池
static MemoryPool<ThreadCache> tcPool;

// 内存申请接口
static void* TcMalloc(size_t size)
{
	// 申请大于256k的内存，直接向PageCache申请
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kPage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPage);
		span->_objSize = alignSize;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
	}
	// 较小内存，走三级缓存
	else
	{
		// 检查当前线程是否有对应的ThreadCache对象，如果没有，就通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			// pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
		}

		 cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		// 调用该线程的ThreadCache对象的Allocate函数申请内存
		return pTLSThreadCache->Allocate(size);
	}
}


// 内存释放接口
static void TcFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	// 大于256k内存，直接释放给PageCache
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	// 较小内存，走三级缓存
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}