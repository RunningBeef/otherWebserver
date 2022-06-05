//
// Created by marvinle on 2019/2/1 7:26 PM.
//

#include "../include/Server.h"
#include "../include/Util.h"

#include <string>
#include <iostream>
#include <dirent.h>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>  //for signal



std::string basePath = ".";   //默认是程序当前目录


void daemon_run()
{
    int pid;
    signal(SIGCHLD, SIG_IGN);//忽略子进程状态的变化
    //1）在父进程中，fork返回新创建子进程的进程ID；
    //2）在子进程中，fork返回0；
    //3）如果出现错误，fork返回一个负值；
    pid = fork();
    if (pid < 0)
    {
        std:: cout << "fork error" << std::endl;
        exit(-1);
    }
    //父进程退出，子进程独立运行
    else if (pid > 0)
   {
        exit(0);
    }
    //之前parent和child运行在同一个session里,parent是会话（session）的领头进程,
    //parent进程作为会话的领头进程，如果exit结束执行的话，那么子进程会成为孤儿进程，并被init收养。
    //执行setsid()之后,child将重新获得一个新的会话(session)id。
    //这时parent退出之后,将不会影响到child了。
    setsid();//调用setid()将创建一个会话期和新的进程组，调用进程为该会话期和进程组组长，并且该进程没有控制终端。
    int fd;
    fd = open("/dev/null", O_RDWR, 0);//可读可写https://blog.csdn.net/archyli/article/details/78937937
    if (fd != -1)
    {
        dup2(fd, STDIN_FILENO);//fd指向标准输入
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }
//    if (fd > 2)
//        close(fd);
}



int main(int argc, char **argv) {

    int threadNumber = 3;   //  默认线程数
    int port = 80;        // 默认端口
    char tempPath[256];
    int opt;
    const char *str = "t:p:r:d";//选项字符串，如果选项字符后面有 ':' 则说明该选项还有一个相关参数
    bool daemon = false;

    while ((opt = getopt(argc, argv, str))!= -1)//https://www.cnblogs.com/qingergege/p/5914218.html
    {
        switch (opt)
        {
            case 't':
            {
                threadNumber = atoi(optarg);//指定线程个数
                break;
            }
            case 'r':
            {
                int ret = check_base_path(optarg);//检查路径
                if (ret == -1) {
                    printf("Warning: \"%s\" 不存在或不可访问, 将使用当前目录作为网站根目录\n", optarg);
                    if(getcwd(tempPath, 300) == NULL)
                    {
                        perror("getcwd error");
                        basePath = ".";
                    }
                    else
                    {
                        basePath = tempPath;
                    }
                    break;
                }
                if (optarg[strlen(optarg)-1] == '/') {
                    optarg[strlen(optarg)-1] = '\0';
                }
                basePath = optarg;
                break;
            }
            case 'p':
            {
                // FIXME 端口合法性校验
                port = atoi(optarg);
                break;
            }
            case 'd':
            {
                daemon = true;
                break;
            }

            default: break;
        }
    }

    if (daemon)
        daemon_run();


    //  输出配置信息
    {
      printf("*******LC WebServer 配置信息*******\n");
      printf("端口:\t%d\n", port);
      printf("线程数:\t%d\n", threadNumber);
      printf("根目录:\t%s\n", basePath.c_str());
    }
    handle_for_sigpipe();

    HttpServer httpServer(port);//构造时 进行 创建socket 命名socket 监听socket
    httpServer.run(threadNumber);//创建线程池，进行
}
