#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

// 从中央缓存CentralCache获取内存块
// 接受两个参数：ThreadCache自由链表对应的桶索引和想获取的内存块大小

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈调节算法
	// 1、最开始不会一次向central cache一次批量要太多，因为要太多了可能用不完
	// 2、如果你不要这个size大小内存需求，那么batchSize就会不断增长，直到上限
	// 3、size越大，一次向central cache要的batchSize就越小
	// 4、size越小，一次向central cache要的batchSize就越大
	size_t batchSize= min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchSize)
		++_freeLists[index].MaxSize();

	void* start = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObjToThread(start, batchSize, size);

	// 至少获取一块
	assert(actualNum > 0);

	if (1 == actualNum)
		assert(start == end);
	else     // 多余块保存到_freeLists
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);

	return start;
}

// 线程内分配内存
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);

	// 计算内存块的对齐大小alignSize和内存块所在的自由链表的下标index
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	// _freeLists[index]非空，直接 _freeLists[index]取出一个内存块
	if (!_freeLists[index].Empty())
		return _freeLists[index].Pop();
	// _freeLists[index]已空，调用FetchFromCentralCache向中心缓存申请内存块
	else
		FetchFromCentralCache(index, alignSize);
}

// 线程内回收内存
// 参数：内存块指针ptr 和内存块大小size
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// 计算映射的自由链表桶index，回收至_freeLists[index]
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	// 当链表长度大于一次批量申请的内存时，就开始还一段list给CentralCache
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
		FreeToCentralCache(_freeLists[index], size);	
}

// ThreadCache归还内存块到CentralCache
void ThreadCache::FreeToCentralCache(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;

	list.PopRange(start, list.MaxSize());

	// 往上层空间传递
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}