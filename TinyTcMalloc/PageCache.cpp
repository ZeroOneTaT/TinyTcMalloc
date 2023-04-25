#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

// 声明单例
PageCache PageCache::_sInst;

// 获取一个K页的span
/*
1、检查第k个桶里面是否存在空闲span，若存在就直接返回；
2、若不存在，则检查后面的桶里面是否存在更大的span，
	若存在则切分一个k页的span返回，剩下的页数的span放到对应的桶里；
3、若后面的桶里也不存在空闲span，则向系统堆申请一个大小为128页的span，
	并把它放到对应的桶里，再递归调用自己，直至获取到一个k页的span。
*/
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);
	// k > 128
	if (k > NPAGES - 1)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;		// 页号：地址右移PAGE_SHIFT获得
		span->_num = k;									// 页数


		 _idSpanMap[span->_pageID] = span;
		//_idSpanMap.set(span->_pageId, span);

		return span;
	}
	
	// 1、检查第k个桶里面是否存在span
	if (!_spanlists[k].Empty())
	{
		Span* kSpan = _spanlists[k].PopFront();

		// 建立<id, span>映射，方便CentralCache回收内存时，查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_num; ++i)
		{
			_idSpanMap[kSpan->_pageID + i] = kSpan;
		}

		return kSpan;
	}

	// 2、第k个桶为空，检查后续桶
	for (size_t i = k + 1; i < NPAGES; ++i)
	{
		if (!_spanlists[i].Empty())
		{
			Span* nSpan = _spanlists[i].PopFront();
			Span* kSpan = _spanPool.New();

			// 切分kSpan大小内存，剩余挂载到对应映射位置
			kSpan->_pageID = nSpan->_pageID;
			kSpan->_num = k;

			nSpan->_pageID += k;
			nSpan->_num -= k;

			// 剩余部分放入对应哈希桶
			_spanlists[nSpan->_num].PushFront(nSpan);

			// 存储nSpan的首尾页号跟nSpan映射， 方便PageCache回收内存时进行的合并查找
			// 因为没被中心缓存拿走，只需要标记首尾
			 _idSpanMap[nSpan->_pageID] = nSpan;
			 _idSpanMap[nSpan->_pageID + nSpan->_num - 1] = nSpan;


			 for (PAGE_ID i = 0; i < kSpan->_num; ++i)
			 {
				 _idSpanMap[kSpan->_pageID + i] = kSpan;
			 }

			 return kSpan;
		}
	}
	// 3、无合适span，向堆申请一个128页的span
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);

	// 通过将 ptr 地址强制转换为 PAGE_ID 类型，
	// 再使用二进制位运算符 >> 将指针的地址右移 PAGE_SHIFT 位
	// 最终得到的结果就是这个指针所在的“页的编号”
	bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_num = NPAGES - 1;

	_spanlists[bigSpan->_num].PushFront(bigSpan);

	return NewSpan(k);// 递归调用自己，这一次一定能成功！

}


// 根据传入的指针地址找到对应映射的Span，
// 如果不存在说明给的地址有问题（要么程序逻辑实现错误，要么外面乱传）
Span* PageCache::MapObjectToSpan(void* obj)
{
	// 根据地址算出页号
	PAGE_ID idx = (PAGE_ID)obj >> PAGE_SHIFT;
	
	// 注意：查找span，可能存在另外的线程进行插入和删除操作，需要加锁
	// 自动上锁与解锁
	std::unique_lock<std::mutex> lock(_pageMtx);

	auto ret = _idSpanMap.find(idx);
	if (ret != _idSpanMap.end())
	{
		return ret->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

// 向上回收span，并且向上和向下进行内存合并，解决外碎片问题
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 大于128页直接归还堆区
	if (span->_num > NPAGES - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		_spanPool.Delete(span);
		_idSpanMap.erase(span->_pageID);
		return;
	}

	// 向前合并
	while (true)
	{
		PAGE_ID prevID = span->_pageID - 1;

		auto ret = _idSpanMap.find(prevID);
		// 前向span为空，不合并
		if (ret == _idSpanMap.end())
			break;

		// 前向span被使用，不合并
		Span* prevSpan = ret->second;
		if (prevSpan->_isUsed == true)
			break;

		// 合并后超过128页，不合并
		if (prevSpan->_num + span->_num > NPAGES - 1)
			break;

		// 满足合并条件，执行合并
		span->_pageID = prevSpan->_pageID;
		span->_num += prevSpan->_num;

		_spanlists[prevSpan->_num].Erase(prevSpan);
		_spanPool.Delete(prevSpan);

	}

	// 向后合并
	while (true)
	{
		PAGE_ID nextID = span->_pageID + span->_num;

		auto ret = _idSpanMap.find(nextID);
		// 后向span为空，不合并
		if (ret == _idSpanMap.end())
			break;

		// 前向span被使用，不合并
		Span* nextSpan = ret->second;
		if (nextSpan->_isUsed == true)
			break;

		// 合并后超过128页，不合并
		if (nextSpan->_num + span->_num > NPAGES - 1)
			break;

		// 满足合并条件，执行合并
		span->_num += nextSpan->_num;

		_spanlists[nextSpan->_num].Erase(nextSpan);
		_spanPool.Delete(nextSpan);
	}

	_spanlists[span->_num].PushFront(span);			// 合并后的span重新挂载到对应的双向链表
	span->_isUsed = true;

	_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_num - 1] = span;
}