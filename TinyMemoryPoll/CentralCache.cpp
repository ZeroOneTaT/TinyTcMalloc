#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;	// 单例模式声明

// 从CentralCache取空闲Span,若无，向PageCache申请
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}

	// 解锁，方便其他线程释放对象
	list.unlock();

	// 无空闲Span，向PageCache申请
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUsed = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// 对新获取span切分，无需加锁，单例模式，其余线程无法访问该span

	// 计算span大块内存的起始地址及大小(字节)
	char* start = (char*)(span->_pageID << PAGE_SHIFT);
	size_t bytes = span->_num << PAGE_SHIFT;
	char* end = start + bytes;

	// 切分并链接
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	int i = 1;
	while (start < end)
	{
		++i;
		NextObj(tail) = start;
		tail = NextObj(tail);
		start += size;
	}
	NextObj(tail) = nullptr;

	// 切分span后，加锁挂载到CentralCache对应的哈希桶中
	list.lock();
	list.PushFront(span);

	return span;
}

// 从CentralCache获取一定数量内存对象给ThreadCache
size_t CentralCache::FetchRangeObjToThread(void*& start, size_t batchNum, size_t size)
{
	// CentralCache哈希桶的映射规则和ThreadCache哈希桶映射规则一致
	size_t index = SizeClass::Index(size);
	// 加锁
	_spanlists[index].lock();
	Span* span = GetOneSpan(_spanlists[index], size);
	assert(span);					// 检查获取的span是否为空
	assert(span->_freeList);		// 检查获取的span的自由链表是否为空

	// 申请到的span,尽可能切分
	void* end = span->_freeList;
	start = end;
	assert(start);

	size_t actualNum = 1;			// 实际数量
	while (actualNum < batchNum && NextObj(end))
	{
		++actualNum;
		end = NextObj(end);
	}

	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_usedCount += actualNum;

	// 解锁
	_spanlists[index].unlock();
	return actualNum;
}

// 根据ThreadCache返回的自由链表，根据其映射对应的span插入
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	// 根据start查询对应的span
	size_t index = SizeClass::Index(size);

	_spanlists[index].lock();
	
	while (start)
	{
		// 把start开头的这一串自由链表内存归还对应span,一次循环还一个，一直还
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		void* next = NextObj(start);
		// 头插
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_usedCount--;

		// span的切分出去的所有小块内存全部会搜，清理span，将完整span交给page
		// 这个span就可以再回收给PageCache，PageCache可以再尝试去做前后页的合并
		if (span->_usedCount == 0)
		{
			// 先从spanlists上删除span
			_spanlists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = span->_prev = nullptr;

			_spanlists[index].unlock();

			// 释放span给page cache时，span已经从_spanLists[index]删除了，不需要再加桶锁了
			// 这时把桶锁解掉，使用page cache的锁就可以了,方便其他线程申请/释放内存
			_spanlists[index].unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanlists[index].lock();
		}
		start = next;
	}
	_spanlists[index].unlock();
}