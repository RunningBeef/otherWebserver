//
// Created by marvinle on 2019/2/25 8:49 PM.
//

//#ifndef WEBSERVER_CONDITION_H
//#define WEBSERVER_CONDITION_H
#pragma once

#include <pthread.h>
#include "MutexLock.h"
#include "noncopyable.h"



class Condition : public noncopyable {
public:
    explicit Condition(MutexLock &mutex): mutex_(mutex){
        pthread_cond_init(&cond_, NULL);
    }
    ~Condition() {
        pthread_cond_destroy(&cond_);
    }

    void wait() {//现用互斥所加锁，然后将调用线程存入条件变量的等待队列中，再给互斥锁解锁
        pthread_cond_wait(&cond_, mutex_.getMutex());
    }

    void notify() {
        pthread_cond_signal(&cond_);
    }

    void notifyAll() {
        pthread_cond_broadcast(&cond_);
    }

private:
    MutexLock &mutex_;//保护条件变量的互斥锁，确保操作的原子性
    pthread_cond_t cond_;
};

//#endif //WEBSERVER_CONDITION_H
