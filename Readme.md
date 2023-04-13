# C++项目：TinyMemoryPoll

## 项目介绍

本项目旨在实现一个高并发[内存池](https://so.csdn.net/so/search?q=内存池&spm=1001.2101.3001.7020)，参考了Google的开源项目[tcmalloc](https://github.com/google/tcmalloc)实现的简化版本。

TinyMemoryPoll的功能主要是实现高效的多线程内存管理。由功能可知，高并发指的是高效的多线程，而内存池则是实现内存管理的。