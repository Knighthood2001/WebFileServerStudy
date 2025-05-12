#include "threadpool.h"
// C++11 线程池实现，它的作用是：用固定数量的线程异步处理任务队列中的工作任务。你调用 enqueue(...) 添加任务，线程池内部的工作线程就会自动去处理。
/**
 * @brief ThreadPool 构造函数
 *
 * 初始化线程池，并启动指定数量的工作线程。
 *
 * @param numThreads 要启动的工作线程数量
 */
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        // 创建工作线程
        workers.emplace_back([this] {
            // 循环处理任务直到停止信号被设置
            while (!stop) {
                std::function<void()> task;
                //支持所有 "可以调用且符合 void() 签名" 的东西，让线程池可以执行各种任务，无论是函数、lambda 还是成员函数，灵活性非常强。
                {
                    // 加锁以保护任务队列
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    // 等待任务队列中有任务或停止信号被设置，只要满足其中一个条件，就唤醒这个线程继续干活
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();  //线程池准备关闭或者任务队列为空
                    });
                    //因此，假设线程池刚启动，任务队列是空的。工作线程跑到上面这一行，等待中，直到有任务被加入或者线程池准备关闭。



                    // 如果停止信号被设置且任务队列为空，则退出循环
                    if (this->stop && this->tasks.empty())
                        return;

                    // 否则，从任务队列中取出任务
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                // 执行任务
                task();
            }
        });
    }
}

/**
 * @brief 将一个任务添加到线程池的任务队列中
 *
 * 将给定的任务添加到线程池的任务队列中，并通知一个等待线程处理任务。
 *
 * @param task 需要添加到任务队列中的任务，类型为std::function<void()>
 */
void ThreadPool::enqueue(std::function<void()> task) {
    { // 加锁代码块
        std::unique_lock<std::mutex> lock(queueMutex); // 创建并锁定互斥锁
        tasks.emplace(std::move(task)); // 将任务加入任务队列
    } // 解锁代码块
    condition.notify_one(); // 通知一个等待线程处理任务
}

ThreadPool::~ThreadPool() {
    // 设置停止标志为true
    stop = true;
    // 通知所有等待中的线程
    condition.notify_all();
    // 遍历所有工作线程
    for (std::thread &worker : workers) {
        // 等待每个工作线程结束
        worker.join();
    }
}
