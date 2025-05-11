#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);  //启动 numThreads 个工作线程。explicit 防止隐式类型转换。  
    ~ThreadPool();

    void enqueue(std::function<void()> task); //外部代码通过这个函数提交任务。
    // 任务是一个可以调用的对象，比如 [](){} 或函数指针。

private:
    std::vector<std::thread> workers;  //保存所有工作线程。
    std::queue<std::function<void()>> tasks; //存储所有等待执行的任务。

    std::mutex queueMutex;  //互斥锁，用于同步访问任务队列。用来保护任务队列的读写。
    std::condition_variable condition;  //用来唤醒挂起的线程（当新任务被加入队列）。
    std::atomic<bool> stop;  //原子变量。表示线程池是否应该停止。一旦 stop == true，线程们就会退出循环并终止。
};

#endif // THREADPOOL_H
