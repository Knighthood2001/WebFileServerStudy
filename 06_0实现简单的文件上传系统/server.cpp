// server.cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <cstdio>
#include "threadpool.h"

const int MAX_EVENTS = 10;
const int PORT = 8888;

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handleClient(int clientFd) {
    char buf[1024];
    std::string command;

    // 1. 接收命令（例如：UPLOAD filename\n）
    while (true) {
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if (n <= 0) {
            close(clientFd);
            return;
        }
        command.append(buf, n);
        if (command.find('\n') != std::string::npos) break;
    }

    // 2. 解析命令
    size_t pos = command.find(' ');
    size_t end = command.find('\n');
    if (pos == std::string::npos || end == std::string::npos || pos >= end) {//如果pos和end不合法或者pos在end之前，则命令格式不正确。
        std::cerr << "错误命令格式: " << command << std::endl;
        close(clientFd);
        return;
    }

    std::string action = command.substr(0, pos);  // 获取动作（例如：UPLOAD）
    std::string filename = command.substr(pos + 1, end - pos - 1);  // 获取文件名（例如：example.txt）

    if (action != "UPLOAD") {
        std::cerr << "未知命令: " << command << std::endl;
        close(clientFd);
        return;
    }

    std::cout << "接收上传: " << filename << std::endl;

    // 3. 打开文件准备写入
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        std::cerr << "无法创建文件: " << filename << std::endl;
        close(clientFd);
        return;
    }

    // 4. 处理命令中可能残留的数据部分  检查命令字符串（command）中是否包含命令行结束符（\n）之后的数据，如果有，则将这些数据写入文件。
    std::string leftover = command.substr(end + 1);
    if (!leftover.empty()) {
        fwrite(leftover.data(), 1, leftover.size(), file);
    }

    // 5. 继续接收文件数据
    while (true) {
        ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        fwrite(buf, 1, n, file);  // 将接收到的数据写入文件
    }

    fclose(file);
    close(clientFd);
    std::cout << "文件已保存: " << filename << std::endl;
}

int main() {
    // 创建套接字
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

    // 设置套接字选项，允许地址重用
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 设置服务器地址和端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    // 绑定套接字到指定地址和端口
    bind(listenFd, (sockaddr*)&addr, sizeof(addr));

    // 监听套接字上的连接请求
    listen(listenFd, SOMAXCONN);

    // 设置套接字为非阻塞模式
    setNonBlocking(listenFd);

    // 创建epoll实例
    int epollFd = epoll_create1(0);

    // 初始化epoll事件结构体
    epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listenFd;

    // 将监听套接字添加到epoll实例中
    epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &ev);

    // 创建线程池
    ThreadPool pool(std::thread::hardware_concurrency());

    // 输出服务器启动信息
    std::cout << "服务器启动，端口: " << PORT << std::endl;

    while (true) {
        // 等待事件发生
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);

        // 遍历所有就绪的事件
        for (int i = 0; i < nfds; ++i) {
            // 如果是监听套接字就绪
            if (events[i].data.fd == listenFd) {
                // 接受客户端连接
                sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                int clientFd = accept(listenFd, (sockaddr*)&clientAddr, &clientLen);

                // 设置客户端套接字为非阻塞模式
                setNonBlocking(clientFd);

                // 设置客户端套接字事件为可读和边缘触发
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clientFd;

                // 将客户端套接字添加到epoll实例中
                epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &ev);
            }
            // 如果是其他套接字就绪且可读
            else if (events[i].events & EPOLLIN) {
                int clientFd = events[i].data.fd;

                // 从epoll实例中删除客户端套接字
                epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);

                // 将处理客户端连接的任务添加到线程池中
                pool.enqueue([clientFd]() {
                    handleClient(clientFd);
                });
            }
        }
    }

    // 关闭监听套接字
    close(listenFd);
    return 0;
}
