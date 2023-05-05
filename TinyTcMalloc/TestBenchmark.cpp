#include "TinyTcmalloc.h"
#include <string>
/*
	内存池压力测试文件
*/

// ntimes: 每个线程轮次调用malloc函数的次数
// nworkers: 线程数量
// rounds: 每个线程执行的轮次数量

void BenchmarkMalloc(size_t nworkers, size_t rounds, size_t ntimes)
{
	std::vector<std::thread> vthread(nworkers);
	std::atomic<size_t> malloc_costtime1 = 0;
	std::atomic<size_t> free_costtime1 = 0;
	std::atomic<size_t> malloc_costtime2 = 0;
	std::atomic<size_t> free_costtime2 = 0;

	for (size_t k = 0; k < nworkers; ++k)
	{
		vthread[k] = std::thread([&]()
			{
				std::vector<void*> v1;
				v1.reserve(ntimes);

				std::vector<void*> v2;
				v2.reserve(ntimes);

				for (size_t j = 0; j < rounds; ++j)
				{
					// 申请小内存-->申请较大内存-->释放小内存-->释放大内存
					size_t s_malloc1 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						v1.push_back(malloc(16));
					}
					size_t e_malloc1 = clock();

					size_t s_malloc2 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						v2.push_back(malloc(65 * 1024));
					}
					size_t e_malloc2 = clock();

					size_t s_free1 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						free(v1[i]);
					}
					size_t e_free1 = clock();
					v1.clear();

					size_t s_free2 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						free(v2[i]);
					}
					size_t e_free2 = clock();
					v2.clear();

					malloc_costtime1 += (e_malloc1 - s_malloc1);
					free_costtime1 += (e_free1 - s_free1);

					malloc_costtime2 += (e_malloc2 - s_malloc2);
					free_costtime2 += (e_free2 - s_free2);
				}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	cout << "线程数：" << nworkers << "  轮次/线程数：" << rounds << "  操作次数/轮次：" << ntimes << endl << endl;
	cout << "Malloc Small 操作花费：" << malloc_costtime1 << " ms" << endl;
	cout << "Free Small 操作花费：" << free_costtime1 << " ms" << endl;
	cout << "Malloc && Free Small 操作花费：" << malloc_costtime1 + free_costtime1 << " ms" << endl;
	cout << "Malloc Big 操作花费：" << malloc_costtime2 << " ms" << endl;
	cout << "Free Big 操作花费：" << free_costtime2 << " ms" << endl;
	cout << "Malloc && Free Big 操作花费：" << malloc_costtime2 + free_costtime2 << " ms" << endl;
	cout << "Malloc All 操作花费：" << malloc_costtime1 + malloc_costtime2 << " ms" << endl;
	cout << "Free All 操作花费：" << free_costtime1 + free_costtime2 << " ms" << endl;
	cout << "Malloc && Free All 操作花费：" << malloc_costtime1 + free_costtime1 + malloc_costtime2 + free_costtime2 << " ms" << endl;


}


// ntimes: 每个线程轮次调用malloc函数的次数
// nworkers: 线程数量
// rounds: 每个线程执行的轮次数量
void BenchmarkTcMalloc(size_t nworkers, size_t rounds, size_t ntimes)
{
	std::vector<std::thread> vthread(nworkers);
	std::atomic<size_t> malloc_costtime1 = 0;
	std::atomic<size_t> free_costtime1 = 0;
	std::atomic<size_t> malloc_costtime2 = 0;
	std::atomic<size_t> free_costtime2 = 0;

	for (size_t k = 0; k < nworkers; ++k)
	{
		vthread[k] = std::thread([&]() 
			{
				std::vector<void*> v1;
				v1.reserve(ntimes);

				std::vector<void*> v2;
				v2.reserve(ntimes);

				for (size_t j = 0; j < rounds; ++j)
				{
					// 申请小内存-->申请较大内存-->释放小内存-->释放大内存
					size_t s_malloc1 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						v1.push_back(TcMalloc(16));
					}
					size_t e_malloc1 = clock();

					size_t s_malloc2 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						v2.push_back(TcMalloc(65 * 1024));
					}
					size_t e_malloc2 = clock();

					size_t s_free1 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						TcFree(v1[i]);
					}
					size_t e_free1 = clock();
					v1.clear();

					size_t s_free2 = clock();
					for (size_t i = 0; i < ntimes; i++)
					{
						TcFree(v2[i]);
					}
					size_t e_free2 = clock();
					v2.clear();

					malloc_costtime1 += (e_malloc1 - s_malloc1);
					free_costtime1 += (e_free1 - s_free1);

					malloc_costtime2 += (e_malloc2 - s_malloc2);
					free_costtime2 += (e_free2 - s_free2);
				}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	cout << "线程数：" << nworkers << "  轮次/线程数：" << rounds << "  操作次数/轮次：" << ntimes << endl << endl;
	cout << "TcMalloc Small 操作花费：" << malloc_costtime1 << " ms" << endl;
	cout << "TcFree Small 操作花费：" << free_costtime1 << " ms" << endl;
	cout << "TcMalloc && TcFree Small 操作花费：" << malloc_costtime1 + free_costtime1 << " ms" << endl;
	cout << "TcMalloc Big 操作花费：" << malloc_costtime2 << " ms" << endl;
	cout << "TcFree Big 操作花费：" << free_costtime2 << " ms" << endl;
	cout << "TcMalloc && TcFree Big 操作花费：" << malloc_costtime2 + free_costtime2 << " ms" << endl;
	cout << "TcMalloc All 操作花费：" << malloc_costtime1 + malloc_costtime2 << " ms" << endl;
	cout << "TcFree All 操作花费：" << free_costtime1 + free_costtime2<< " ms" << endl;
	cout << "TcMalloc && TcFree All 操作花费：" << malloc_costtime1 + free_costtime1 + malloc_costtime2 + free_costtime2 << " ms" << endl;
}



int main()
{
	size_t nworkers = 4;
	size_t rounds = 10;
	size_t ntimes = 2000;
	
	cout << "==========================================================" << endl;
	BenchmarkMalloc(nworkers, rounds, ntimes);
	cout << endl << endl;
	
	cout << "==========================================================" << endl;
	BenchmarkTcMalloc(nworkers, rounds, ntimes);
	cout << "==========================================================" << endl;

	return 0;
}