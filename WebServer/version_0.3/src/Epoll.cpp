//
// Created by marvinle on 2019/2/26 8:08 PM.
//

#include "../include/Epoll.h"
#include "../include/Util.h"
#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <cstdio>



std::unordered_map<int, std::shared_ptr<HttpData>> Epoll::httpDataMap;
const int Epoll::MAX_EVENTS = 10000;
epoll_event *Epoll::events;

// 可读 | ET模 | 保证一个socket连接在任一时刻只被一个线程处理
const __uint32_t Epoll::DEFAULT_EVENTS =  (EPOLLIN | EPOLLET | EPOLLONESHOT);

TimerManager Epoll::timerManager;



//创建指向事件表的一个文件描述符，并且创建事件数组
int Epoll::init(int max_events) {
    int epoll_fd = ::epoll_create(max_events);
    if (epoll_fd == -1) {
        std::cout << "epoll create error" << std::endl;
        exit(-1);
    }
    events = new epoll_event[max_events];
    return epoll_fd;
}

//往事件表中注册fd上的事件。 把fd和httpdata捆绑在一起
int Epoll::addfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData> httpData) {
    epoll_event event;
    event.events = (EPOLLIN|EPOLLET);//数据可读事件 边沿触发
    event.data.fd = fd;//事件所属的文件描述符
    // 增加httpDataMap
    httpDataMap[fd] = httpData;
    int ret = ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    if (ret < 0) {
        std::cout << "epoll add error" << endl;
        // 释放httpData
        httpDataMap[fd].reset();
        return -1;
    }
    return 0;
}

//修改fd上的事件
int Epoll::modfd(int epoll_fd, int fd, __uint32_t events, std::shared_ptr<HttpData> httpData) {
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    // 每次更改的时候也更新 httpDataMap
    httpDataMap[fd] = httpData;
    int ret = ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    if (ret < 0) {
        std::cout << "epoll mod error" << endl;
        // 释放httpData
        httpDataMap[fd].reset();
        return -1;
    }
    return 0;
}

//删除fd上的注册事件
int Epoll::delfd(int epoll_fd, int fd, __uint32_t events) {
    epoll_event event;
    event.events = events;
    event.data.fd = fd;
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event);
    if (ret < 0) {
        std::cout << "epoll del error" << endl;
        return -1;
    }
    auto it = httpDataMap.find(fd);
    if (it != httpDataMap.end()) {
        httpDataMap.erase(it);
    }
    return 0;
}

void Epoll::handleConnection(const ServerSocket &serverSocket) {//处理客户端连接请求

    std::shared_ptr<ClientSocket> tempClient(new ClientSocket);
    // epoll 是ET模式，循环接收连接
    // 需要将listen_fd设置为non-blocking

    while(serverSocket.accept(*tempClient) > 0) {//从监听队列接受连接，设置新的连接socket到tempClient
        // 设置非阻塞
        int ret = setnonblocking(tempClient->fd);
        if (ret < 0) {
            std::cout << "setnonblocking error" << std::endl;
            tempClient->close();
            continue;
        }

        // FIXME 接受新客户端 构造HttpData并添加定时器

        // 在这里做限制并发, 暂时未完成

        std::shared_ptr<HttpData> sharedHttpData(new HttpData);
        sharedHttpData->request_ = std::shared_ptr<HttpRequest>(new HttpRequest());
        sharedHttpData->response_ = std::shared_ptr<HttpResponse>(new HttpResponse());

        std::shared_ptr<ClientSocket> sharedClientSocket(new ClientSocket());
        sharedClientSocket.swap(tempClient);//交换指针
        sharedHttpData->clientSocket_ =  sharedClientSocket;
        sharedHttpData->epoll_fd = serverSocket.epoll_fd;//给新的连接socket设置内核事件表

        std::cout << "\n\n\nlisten socket !!!!!!!!!!!!!!" << sharedClientSocket->fd << "\n\n\n";
        //往内核事件表中注册事件 如果注册失败要把sharedHttpData 中的信息释放
        addfd(serverSocket.epoll_fd, sharedClientSocket->fd, DEFAULT_EVENTS, sharedHttpData);
        // FIXME 默认超时时间5 秒测试添加定时器
        timerManager.addTimer(sharedHttpData, TimerManager::DEFAULT_TIME_OUT);
    }

}

std::vector<std::shared_ptr<HttpData>> Epoll::poll(const ServerSocket &serverSocket, int max_event, int timeout) {
    int event_num = epoll_wait(serverSocket.epoll_fd, events, max_event, timeout);//返回就绪文件描述符（有事件发生）个数
    if (event_num < 0) {//-1失败
        std::cout << "epoll_num=" << event_num << std::endl;
        std::cout << "epoll_wait error" << std::endl;
        std::cout << errno << std::endl;
        exit(-1);
    }


    std::vector<std::shared_ptr<HttpData>> httpDatas;
    // 遍历events集合
    for(int i = 0; i < event_num; i++) {
        int fd = events[i].data.fd;

        // 监听描述符
        if (fd == serverSocket.listen_fd) {
            handleConnection(serverSocket);//如果是监听socket 处理新的连接
        }
        else {
            std::cout << "get method!!!!!!!!!!!!!!!!!!!!!" << std::endl;
             // 出错的描述符，移除定时器， 关闭文件描述符
             // EPOLLERR 错误  EPOLLRDHUP TCP连接被对方关闭 EPOLLHUP 管道写段关闭
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLHUP)) {
                auto it = httpDataMap.find(fd);//？
                if (it != httpDataMap.end()) {
                    // 将HttpData节点和TimerNode的关联分开，这样HttpData会立即析构，在析构函数内关闭文件描述符等资源
                    it->second->closeTimer();
                    //httpDataMap.erase(it);
                }
                continue;
            }

            auto it = httpDataMap.find(fd);
            if (it != httpDataMap.end()) {
                //数据可读 或高优先级数据可读
                if ((events[i].events & EPOLLIN) || (events[i].events & EPOLLPRI)) {//？
                    httpDatas.push_back(it->second);
                    //std::cout << "定时器中找到:" << fd << std::endl;
                    // 清除定时器 HttpData.closeTimer()
                    it->second->closeTimer();
                    httpDataMap.erase(it);
                }
            } else {
                std::cout << "长连接第二次连接未找到" << std::endl;
                ::close(fd);
                continue;
            }
            // 这里有个问题是 TimerNode正常超时释放时

        }
    }
    return httpDatas;
}