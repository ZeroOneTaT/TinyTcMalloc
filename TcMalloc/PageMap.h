#pragma once
/*
	基数树的设计，避免使用锁带来的额外开销
*/
#include "Common.h"
//#include "MemoryPool.h"

// 单层基数树
template <int BITS>
class PageMap1 {
private:
	// 按页号直接映射，BITS = 32 - PAGE_SHIFT = 19位
	// 即32位系统最多2^32位字节，每页2^PAGE_SHIFT，最多有2^BITS页
	static const int LENGTH = 1 << BITS;
	void** _array;
public:
	typedef uintptr_t Number;

	// 显式定义构造函数，防止隐式转换
	explicit PageMap1()
	{
		// 计算数组开辟空间所需的大小
		size_t size = sizeof(void*) << BITS;
		size_t alignedSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		// 直接向堆区申请空间
		_array = (void**)SystemAlloc(alignedSize >> PAGE_SHIFT);
		memset(_array, 0, size);
	}

	// 返回当前key的值，未分配则返回NULL
	void* get(Number key) const
	{
		if ((key >> BITS) > 0)
			return NULL;
		return _array[key];
	}

	// 建立key-value映射
	void set(Number key, void* value)
	{
		_array[key] = value;
	}
};


// 二层基数树
template <int BITS>
class PageMap2 
{
private:
	// 将一层基数树的BITS<19位>分解成5 + 14位两层基数树.
	static const int ROOT_BITS = 5;
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// 叶子节点定义
	struct Leaf 
	{
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // 根节点指向叶子节点
	void* (*allocator_)(size_t);          // 内存分配器

public:
	typedef uintptr_t Number;

	explicit PageMap2() {
		memset(root_, 0, sizeof(root_));

		_preAllocatememory();
	}

	void* get(Number k) const {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}

	bool _ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// 检测是否越界
			if (i1 >= ROOT_LENGTH)
				return false;

			// 创建二级节点
			if (root_[i1] == NULL) {
				static	MemoryPool<Leaf>	leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// 跳跃到下一个根节点位置
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void _preAllocatememory() {
		// 预分配空间
		_ensure(0, 1 << BITS);
	}
};

// 三层基数树
// 64位系统使用，本项目主要在32位系统运行，故不考虑三层基数树
template <int BITS>
class PageMap3 {
private:
	// How many bits should we consume at each interior level
	static const int INTERIOR_BITS = (BITS + 2) / 3; // Round-up
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

	// How many bits should we consume at leaf level
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Interior node
	struct Node {
		Node* ptrs[INTERIOR_LENGTH];
	};

	// Leaf node
	struct Leaf {
		void* values[LEAF_LENGTH];
	};

	Node* root_;                          // Root of radix tree
	void* (*allocator_)(size_t);          // Memory allocator

	Node* NewNode() {
		Node* result = reinterpret_cast<Node*>((*allocator_)(sizeof(Node)));
		if (result != NULL) {
			memset(result, 0, sizeof(*result));
		}
		return result;
	}

public:
	typedef uintptr_t Number;

	explicit PageMap3(void* (*allocator)(size_t)) {
		allocator_ = allocator;
		root_ = NewNode();
	}

	void* get(Number k) const {
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 ||
			root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL) {
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3];
	}

	void set(Number k, void* v) {
		ASSERT(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
	}

	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

			// Check for overflow
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
				return false;

			// Make 2nd level node if necessary
			if (root_->ptrs[i1] == NULL) {
				Node* n = NewNode();
				if (n == NULL) return false;
				root_->ptrs[i1] = n;
			}

			// Make leaf node if necessary
			if (root_->ptrs[i1]->ptrs[i2] == NULL) {
				Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() {
	}
};