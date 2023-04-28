# C++项目：TinyMemoryPoll

## 1.项目介绍

本项目旨在实现一个高并发[内存池](https://so.csdn.net/so/search?q=内存池&spm=1001.2101.3001.7020)，参考了Google的开源项目[tcmalloc](https://github.com/google/tcmalloc)实现的简化版本。

TinyMemoryPoll的功能主要是实现高效的多线程内存管理。由功能可知，高并发指的是高效的多线程，而内存池则是实现内存管理的。

## 2.开发环境

i7-4790k、Windows10专业版、Visual Studio 2022 Community

## 3.掌握知识

该项目要求读者掌握**C/C++**、**数据结构（链表和哈希桶）**、操作系统中的**内存管理**、**单例设计模式**、**多线程**以及**互斥锁**相关知识。

- **池化技术**

  **池化技术是指程序预先向系统申请过量的资源，并对这些资源进行合理管理，避免需要使用频繁的申请和释放资源导致的开销。**

  常见的池化技术应用还有**数据结构池**、**线程池**、**对象池**、**连接池**等，读者可自行了解。

- **内存池**

  内存池指的是程序预先向操作系统申请足够大的一块内存空间；此后，程序中需要申请内存时，不需要直接向操作系统申请，而是直接从内存池中获取进行使用；同理，程序释放内存时，也不是将内存直接还给操作系统，而是将内存归还给内存池，以备下次被程序申请使用。当程序退出（或者特定时间）时，内存池才将之前申请内存池中的全部内存真正释放。

- **内存池主要解决问题**

  - 内存池主要解决的是**频繁的内存分配和释放操作所导致的效率问题和内存碎片(不了解的可以参考[浅谈内存碎片](https://blog.csdn.net/fdk_lcl/article/details/89482835))问题**。传统的内存分配方式每次都需要向操作系统申请一块内存，而释放内存时也需要向操作系统归还内存。这种方式存在以下问题：
    1. **内存分配和释放操作的开销比较大**，需要耗费一定的时间和资源，会影响程序的性能；
    2. 由于内存分配和释放不可控，可能会导致**内存碎片**问题，降低内存的使用效率；
    3. **内存泄漏**问题，由于程序无法确定何时需要释放内存，可能导致内存泄漏和资源浪费问题。
  
  ​	内存池通过预先分配一块内存池，并且把内存池分成多个大小相等的内存块，每次从内存池中取出内存块使用，使用完后再归还给内存池，这样就可以  避免了频繁的内存分配和释放操作。同时，内存池还可以提高内存的使用效率，避免内存碎片问题的发生，而且还可以避免内存泄漏问题。
  
- **malloc解析**

  实际上，malloc函数属于C语言的库函数，并非是系统调用。当我们使用malloc函数动态申请内存时，malloc函数会根据程序申请的内存大小（一般以128k为分界线）选择调用brk()系统调用在堆区申请内存或者mmap()系统调用在文件映射区申请内存。偷一下懒，这里放一下[小林coding](https://xiaolincoding.com/)的示意图：


​	![img](https://cdn.xiaolincoding.com/gh/xiaolincoder/ImageHost/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F/%E5%86%85%E5%AD%98%E7%AE%A1%E7%90%86/brk%E7%94%B3%E8%AF%B7.png)

![img](https://cdn.xiaolincoding.com/gh/xiaolincoder/ImageHost/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F/%E5%86%85%E5%AD%98%E7%AE%A1%E7%90%86/brk%E7%94%B3%E8%AF%B7.png)

## 4.项目思维导图



## 5.整体设计

### 5.1 定长内存池

malloc函数适用于各种场景下的内存分配任务，但是**通用往往意味着各方面都不够完美**，比如malloc函数在频繁进行小块内存分配和释放的场景下往往会具有较高的内存调用开销和内存碎片化问题，该项目设计一个定长内存池来优化特定内存申请效率(**Note:内存池技术不适用需要大量申请大块内存的场景**)。

![image-20221231222658167](https://img-blog.csdnimg.cn/img_convert/bbd073cc7724dd20913168d6426f6ce4.png)

#### 适应平台的指针方案

本项目需要选取一块对象内存的前n个内存(32位系统n=4，64位系统n=8)来存放指向下一块释放回来的自由对象内存的指针，为了实现代码的平台通用性，我们可以将对象内存强制转换成__void**__类型，再对这个二级指针解引用即可取出我们需要的当前对象内存的前n个内存。该技巧的核心思想是利用编译器对不同类型在内存中占据空间的大小进行处理，对象内存的地址强制类型转换成__void**__类型，这样就相当于把该对象的地址存放在了一个8个字节（64位系统）或者4个字节（32位系统）的内存单元中。

该操作在本项目中会被频繁使用，为了提高调用执行效率，将其封装成[内联函数](https://www.runoob.com/cplusplus/cpp-inline-functions.html)方便调用：

```c++
// 内存指针转换 4/32位 8/32位
static inline void*& NextObj(void* obj) {
	return *(void**)obj;
}
```

C/C++中申请内存一般使用malloc/new，本项目为了脱离使用malloc函数，直接将Windows操作系统提供的虚拟内存申请系统调用函数VirtualAlloc封装成自己的内存申请函数：

```c++
// 自定义内存申请函数
inline static void* SystemAlloc(size_t kPage)
{
	#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kPage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	#else
		//Linux下brk mmap等
	#endif // _WIN32

	//抛出异常
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}
```

简单介绍一下VirtualAlloc函数：

>```c++
>LPVOID VirtualAlloc(
>  LPVOID lpAddress,
>  SIZE_T dwSize,
>  DWORD  flAllocationType,
>  DWORD  flProtect
>);
>```
>
>其中：
>
>- lpAddress：指定欲保留或提交页面的起始地址。如果为0，则表示让系统决定；如果非0，则要求系统从这个地址开始分配给这个进程。
>- dwSize：欲提交或保留的内存大小，单位是字节。
>- flAllocationType：内存分配类型标志位，包含以下几个值之一：
>  - MEM_COMMIT：提交已经被保留的内存页，此时内存内容已经被清空。
>  - MEM_RESERVE：保留指定大小的虚拟地址空间而不进行物理存储分配。一旦保留了地址空间，就可以在后续的操作中执行提交操作（即将保留的内存页提交，使其与物理存储关联），也可以取消保留操作，释放掉相应的地址空间。
>- flProtect：要应用于保留或提交的内存页所需的访问权限和页面属性（例如读、写、执行等），支持一系列参数，如 PAGE_READWRITE 表示可读可写，PAGE_EXECUTE 表示可执行，PAGE_EXECUTE_READ 表示可读可执行等。

基于以上的内容,我们实现了我们的定长内存池,代码和测试代码点击下方链接获取:

[定长内存池模块代码](https://github.com/ZeroOneTaT/TinyMemoryPoll/blob/master/TinyMemoryPoll/MemoryPool.h)、[定长内存池模块测试代码](https://github.com/ZeroOneTaT/TinyMemoryPoll/blob/master/TinyMemoryPoll/TestMemoryPoll.cpp)

测试5000000次申请和释放内存结果如下，我们可以看出，使用定长内存池的代码效率要高于new(malloc)函数:

![TestMemoryPoll.png](https://github.com/ZeroOneTaT/TinyMemoryPoll/blob/master/images/TestMemoryPoll.png?raw=true)

## 6.性能优化

PageCache使用STL容器中的unordered_map来构建`<_pageID，span>`映射时，我们发现`TcMalloc`内存池的内存分配和释放效率要低于直接使用`malloc`和`free`函数，使用`4`个线程并发执行`10`轮，每轮执行申请并释放`10000`次（执行过程：申请16->申请1024->释放16->释放1024）进行性能测试，测试结果如下图所示：

![BenchmarkWithMap](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\BenchmarkWithMap.png)

### 性能瓶颈分析

1.点击vs工具栏的`调试`，打开该工具目录下的`性能探查器`

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis0.png)

2.选择`性能探查器`下的`检测`选项，以监测应用程序相关函数的调用次数和调用时间，并点击下方的`开始`，开始监测

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis1.png)

3.等待监测运行结果并分析

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis2.png)

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis3.png)

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Anaysis4.png)

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis5.png)

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis6.png)

![](D:\ZeroOne\文档\学习\开发\TinyTcMalloc\images\Analysis7.png)

4.通过解析程序的执行过程，我们发现，为了保证操作的原子性，项目在`unordered_map<PAGE_ID, Span*> _idSpanMap`中的锁竞争上浪费了大量性能，这主要是因为unordered_map是线程不安全的，因此多线程下使用时需要加锁，防止使用`<_pageID，span>`映射时其他线程对映射造成修改，改变哈希桶结构而造成数据不一致，而`<_pageID，span>`映射会被多次使用到，大量加锁、解锁操作会导致资源的消耗。

### 性能优化方案

为了突破`<_pageID，span>`映射大量锁操作带来的性能瓶颈，本项目参考google开源的tcmalloc，使用基数树进行优化。对基数树还不了解的小伙伴可以先看这篇博客：[图解基数树(RadixTree)](https://blog.csdn.net/qq_41583040/article/details/130416816)。
