//
// Created by marvinle on 2019/2/1 2:54 PM.
//

//#ifndef WEBSERVER_SERVER_H
//#define WEBSERVER_SERVER_H
#pragma once

#include "Socket.h"
#include "HttpParse.h"
#include "HttpResponse.h"
#include "HttpData.h"
#include <memory>



#define BUFFERSIZE 2048




class HttpServer {
public:
    enum FileState {
        FILE_OK,
        FIlE_NOT_FOUND,
        FILE_FORBIDDEN
    };

public:
    explicit HttpServer(int port = 80, const char *ip = nullptr) : serverSocket(port, ip) {
        serverSocket.bind();
        serverSocket.listen();
    }

    void run(int, int max_queue_size = 10000);/*webserver整体实现*/

    void do_request(std::shared_ptr<void> arg);/*处理socket http请求的函数,*/

private:

    void header(std::shared_ptr<HttpData>);/*设置响应头部*/

    FileState static_file(std::shared_ptr<HttpData>, const char *);

    void send(std::shared_ptr<HttpData>, FileState);

    void getMime(std::shared_ptr<HttpData>);
    void handleIndex();

    ServerSocket serverSocket;
};


//#endif //WEBSERVER_SERVER_H
