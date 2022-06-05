//
// Created by marvinle on 2019/2/25 9:00 PM.
//

//#ifndef WEBSERVER_MUTEXLOCK_H
//#define WEBSERVER_MUTEXLOCK_H

#pragma once

#include <pthread.h>
#include "noncopyable.h"

class MutexLock : public noncopyable {

public:
    MutexLock() {
        pthread_mutex_init(&mutex_, NULL);
    }
    ~MutexLock() {
        pthread_mutex_destroy(&mutex_);
    }
    void lock() {
        pthread_mutex_lock(&mutex_);
    }
    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }
    pthread_mutex_t* getMutex() {
        return &mutex_;
    }
private:
    pthread_mutex_t mutex_;
};

//通过MutexLockGurad的构造和析构 来调用互斥锁的lock和unlock
class MutexLockGuard : public noncopyable {

public:
    explicit MutexLockGuard(MutexLock &mutex): mutex_(mutex) {//explicit 阻止隐式转化
        mutex_.lock();
    }

    ~MutexLockGuard() {
        mutex_.unlock();
    }
private:
    MutexLock &mutex_;
};

//#endif //WEBSERVER_MUTEXLOCK_H
