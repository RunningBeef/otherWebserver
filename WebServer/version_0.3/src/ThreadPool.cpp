//
// Created by marvinle on 2019/2/26 9:42 AM.
//

#include "../include/ThreadPool.h"
#include <iostream>
#include <pthread.h>
#include <sys/prctl.h>



ThreadPool::ThreadPool(int thread_s, int max_queue_s) : max_queue_size(max_queue_s), thread_size(thread_s),
                                                        condition_(mutex_), started(0), shutdown_(0) {//注意这边我们给conditon_用互斥锁初始化了
    if (thread_s <= 0 || thread_s > MAX_THREAD_SIZE) {
        thread_size = 4;
    }

    if (max_queue_s <= 0 || max_queue_s > MAX_QUEUE_SIZE) {
        max_queue_size = MAX_QUEUE_SIZE;
    }
    // 分配空间
    threads.resize(thread_size);

    for (int i = 0; i < thread_size; i++) {
        // 后期可扩展出单独的Thread类，只需要该类拥有run方法即可
        //打入线程标识符  |  新线程属性 | 新线程运行的函数 | 新线程的运行函数的接口(传入this，使得线程运行PthreadPool对象的run函数()
        if (pthread_create(&threads[i], NULL, worker, this) != 0) {
            std::cout << "ThreadPool init error" << std::endl;
            throw std::exception();
        }
        started++;
    }
}

ThreadPool::~ThreadPool() {

}

bool ThreadPool::append(std::shared_ptr<void> arg, std::function<void(std::shared_ptr<void>)> fun) {

    if (shutdown_) {
        std::cout << "ThreadPool has shutdown" << std::endl;
        return false;
    }

    MutexLockGuard guard(this->mutex_);//生命周期结束后自动将mutex_ unlock();
    if (request_queue.size() > max_queue_size) {//判断请求个数
        std::cout << max_queue_size;
        std::cout << "ThreadPool too many requests" << std::endl;
        return false;
    }
    ThreadTask threadTask;
    threadTask.arg = arg;
    threadTask.process = fun;

    request_queue.push_back(threadTask);
//    if (request_queue.size() == 1) {
//        condition_.notify();
//    }
    // 之前是先判断当前队列是否为空，为空才有线程等待在上面，才需要signal
    // 而后发现其实直接signal也没事，因为signal信号就算没有等待在信号上的也没事
    condition_.notify();//唤醒等待条件变量的线程
    return true;
}

void ThreadPool::shutdown(bool graceful) {
    {
        MutexLockGuard guard(this->mutex_);
        if (shutdown_) {
            std::cout << "has shutdown" << std::endl;
        }
        shutdown_ = graceful ? graceful_mode : immediate_mode;
        condition_.notifyAll();
    }
    for (int i = 0; i < thread_size; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            std::cout << "pthread_join error" << std::endl;
        }
    }
}

void *ThreadPool::worker(void *args) {
    ThreadPool *pool = static_cast<ThreadPool *>(args);//将args 强转为 ThtreadPoll * 类型
    // 退出线程
    if (pool == nullptr)
        return NULL;
    prctl(PR_SET_NAME,"EventLoopThread");//为线程指定名字

    // 执行线程主方法
    pool->run();
    return NULL;
}

void ThreadPool::run() {
    while (true) {
        ThreadTask  requestTask;
        {
            MutexLockGuard guard(this->mutex_);
            // 无任务 且未shutdown 则条件等待, 注意此处应使用while而非if
            while (request_queue.empty() && !shutdown_) {//每个线程循环等待工作队列中的任务
                condition_.wait();
            }

            if ((shutdown_ == immediate_mode) || (shutdown_ == graceful_mode && request_queue.empty())) {
                break;
            }
            // FIFO
            requestTask = request_queue.front();
            request_queue.pop_front();
        }
        requestTask.process(requestTask.arg);//调用并运行do_request函数 参数是HttpData
    }
}
