#pragma once
/*
	CentralCache类封装
*/

#include "Common.h"

// 饿汉式的单例设计模式:
// 类的唯一实例在程序启动时就已经被创建出来，
// 并且在整个程序的生命周期内都只有这一个实例
// 饿汉式优点是线程安全，因为实例在程序启动时就已经被创建，
// 在整个程序的生命周期内都只有这一个实例，不会存在多线程竞争的情况。

class CentralCache {
public:
	// 单例模式，获取唯一实例
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}
	// 获取一个非空Span
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	// 从CentralCache取一定数量内存对象给ThreadCache
	size_t FetchRangeObjToThread(void*& start, size_t batchNum, size_t size);

	// 将一定数量的对象释放到中心缓存的span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanlists[NFREELIST];

private:
	// 构造函数、拷贝构造函数私有化
	CentralCache(){}

	CentralCache(const CentralCache&) = delete;

	// 定义静态的变量 _sInst，用于保存 CentralCache 类的唯一实例
	static CentralCache _sInst;
};