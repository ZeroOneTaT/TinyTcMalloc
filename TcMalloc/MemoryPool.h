#pragma once

#include "Common.h"

// 定长内存池类
template <class T> 
class MemoryPool 
{
public:
	// 重定义New
	T* New() 
	{
		T* obj = nullptr;
		// 自由链表非空，以"头删"方式从自由链表取走内存块，重复利用
		if (_freeList != nullptr) 
		{
			obj = (T*)_freeList;
			//_freeList = *(void**)_freeList;
			_freeList = NextObj(_freeList);
		}
		else 
		{
			// 说明自由链表内没有结点
			// 判断剩余的内存是否足够
			
			//当前内存池中没有足以分配的内存，需要申请
			if (_remaineBytes < sizeof(T))
			{
				_remaineBytes = 64 * 1024;
				//_memory = (char*)malloc(_remaineBytes);//申请定长（64Kb）的内存空间
				_memory = (char*)SystemAlloc(_remaineBytes >> PAGE_SHIFT);//申请定长（64Kb）的内存空间
				
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}
			//保证一次分配的空间够存放下当前平台的指针
			size_t offset = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			obj = (T*)_memory;
			_memory += offset;
			_remaineBytes -= offset;
		}

		// 定位new显式调用T类型构造函数:在内存地址obj处创建一个新的T类型的对象，并调用该对象的构造函数。
		// 与普通的new运算符不同的是，它不会使用动态内存分配器来分配内存，而是使用指定的内存地址。
		new(obj)T;
		return obj;
	}
	// 重定义Delete, 回收内存
	void Delete(T* obj)
	{
		// 显示调用obj对象的析构函数，清理空间
		obj->~T();
		// 将obj内存块头插到freeList
		NextObj(obj) = _freeList;
		_freeList = obj;
	}


private:
	char* _memory = nullptr;		// 指向申请的大块内存的指针
	void* _freeList = nullptr;		// 自由链表的头指针，保存当前可以被重复利用的对象
	size_t _remaineBytes = 0;		// 当前内存池中剩余内存空间
};
