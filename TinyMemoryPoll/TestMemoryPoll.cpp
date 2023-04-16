#include "MemoryPool.h"

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode() :_val(0), _left(NULL), _right(NULL) {}
	TreeNode(int x) : _val(x), _left(nullptr), _right(nullptr) {}
};

void TestMemPool()
{
	// 申请释放的轮次
	const size_t Rounds = 5;
	// 每轮申请释放多少次
	const size_t N = 1000000;
	size_t s1 = clock();
	std::vector<TreeNode*> v1;
	v1.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}
	size_t e1 = clock();

	MemoryPool<TreeNode> TNPool;
	size_t s2 = clock();
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < 100000; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t e2 = clock();
	cout << "The Cost Time of New 5x1000000 iters : " << e1 - s1 <<" ms" << endl;
	cout << "The Cost Time of MemoryPool 5x1000000 iters :" << e2 - s2 << " ms" << endl;
}



int main() 
{
	TestMemPool();
	return 0;
}