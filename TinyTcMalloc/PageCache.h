#pragma once
/*
	PageCache类封装
*/

#include "Common.h"
#include "MemoryPool.h"

class PageCache {
public:
	// 饿汉式单例设计模式
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	
	// 获取从内存对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span到PageCache，并合并相邻span
	void ReleaseSpanToPageCache(Span* span);

	// 申请k页的span
	Span* NewSpan(size_t k);

	void lock() { _pageMtx.lock(); }
	void unlock() { _pageMtx.unlock(); }

private:
	std::mutex _pageMtx;							// 锁

	static PageCache _sInst;

	SpanList _spanlists[NPAGES];					//	PageCache的双链表哈希桶，直接按页数映射
	MemoryPool<Span> _spanPool;

	std::unordered_map<PAGE_ID, Span*> _idSpanMap;	// 

	PageCache(){}
	PageCache(const PageCache&) = delete;
};