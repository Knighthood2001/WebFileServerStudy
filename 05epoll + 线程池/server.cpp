#include <iostream>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "threadpool.h"

#define PORT 8888
#define MAX_EVENTS 10
#define BUF_SIZE 1024

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // 创建一个线程池，包含4个线程
    ThreadPool pool(4);

    // 创建一个监听套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置监听套接字为非阻塞模式
    setNonBlocking(listen_fd);

    // 设置服务器地址信息
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 将套接字绑定到服务器地址
    bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    // 开始监听连接请求
    listen(listen_fd, SOMAXCONN);

    // 创建一个epoll实例
    int epoll_fd = epoll_create1(0);

    // 初始化epoll事件结构体和事件数组
    epoll_event ev{}, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    // 将监听套接字添加到epoll实例中
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    // 输出服务器启动信息
    std::cout << "Server started on port " << PORT << "\n";

    // 循环等待并处理事件
    while (true) {
        // 等待事件，返回的是有事件的文件描述符数量
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        // 遍历事件数组
        for (int i = 0; i < nfds; ++i) {
            // 如果是监听套接字的事件
            if (events[i].data.fd == listen_fd) {// 有新的客户端尝试连接！
                // 接受连接  
                int conn_fd = accept(listen_fd, nullptr, nullptr);
                // 设置连接套接字为非阻塞模式
                setNonBlocking(conn_fd);

                // 初始化epoll事件结构体
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_fd;
                // 将连接套接字添加到epoll实例中
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);

                // 输出接受连接的信息
                std::cout << "Accepted new connection: " << conn_fd << "\n";
            } else {
                // 获取客户端套接字
                int client_fd = events[i].data.fd;
                // 将处理客户端请求的任务添加到线程池中
                pool.enqueue([client_fd]() {
                    char buf[BUF_SIZE];
                    // 循环读取客户端数据
                    while (true) {
                        int n = read(client_fd, buf, BUF_SIZE);
                        // 如果读取到0字节，表示客户端关闭连接
                        if (n == 0) {
                            std::cout << "Client disconnected: " << client_fd << "\n";
                            close(client_fd);
                            break;
                        } else if (n < 0) {
                            // 如果读取错误，且错误码为EAGAIN或EWOULDBLOCK，则跳出循环
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            // 输出读取错误并关闭连接
                            perror("read error");
                            close(client_fd);
                            break;
                        } else {
                            // 输出接收到的数据并回写给客户端
                            std::cout << "Thread " << std::this_thread::get_id()
                                      << " received: " << std::string(buf, n) << "\n";
                            write(client_fd, buf, n);
                        }
                    }
                });
            }
        }
    }

    // 关闭监听套接字
    close(listen_fd);
    return 0;
}
