// //
// // Created by wu on 25-5-10.
// //
// // epoll_server.cpp
// #include <iostream>
// #include <unistd.h>
// #include <cstring>
// #include <fcntl.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <sys/epoll.h>

// #define MAX_EVENTS 10
// #define BUF_SIZE 1024
// #define PORT 8888

// // 将 fd 设置为非阻塞
// void setNonBlocking(int fd) {
//     int flags = fcntl(fd, F_GETFL, 0);
//     fcntl(fd, F_SETFL, flags | O_NONBLOCK);
// }

// int main() {
//     int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
//     setNonBlocking(listen_fd);

//     sockaddr_in server_addr{};
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     server_addr.sin_addr.s_addr = INADDR_ANY;

//     bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr));
//     listen(listen_fd, SOMAXCONN);

//     int epoll_fd = epoll_create1(0);

//     epoll_event ev{}, events[MAX_EVENTS];
//     ev.events = EPOLLIN;
//     ev.data.fd = listen_fd;
//     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

//     char buf[BUF_SIZE];

//     std::cout << "Server started on port " << PORT << "\n";

//     while (true) {
//         int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//         for (int i = 0; i < nfds; ++i) {
//             if (events[i].data.fd == listen_fd) {
//                 // 有新连接到来
//                 sockaddr_in client_addr{};
//                 socklen_t client_len = sizeof(client_addr);
//                 int conn_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
//                 if (conn_fd < 0) {
//                     perror("accept");
//                     continue;
//                 }
//                 setNonBlocking(conn_fd);

//                 ev.events = EPOLLIN | EPOLLET; // 边缘触发
//                 ev.data.fd = conn_fd;
//                 epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);

//                 std::cout << "Accepted new connection: " << conn_fd << "\n";
//             } else {
//                 // 客户端有数据可读
//                 int client_fd = events[i].data.fd;
//                 while (true) {
//                     int n = read(client_fd, buf, BUF_SIZE);
//                     if (n == 0) {
//                         std::cout << "Client disconnected: " << client_fd << "\n";
//                         close(client_fd);
//                         break;
//                     } else if (n < 0) {
//                         if (errno == EAGAIN || errno == EWOULDBLOCK)
//                             break; // 没有更多数据
//                         perror("read error");
//                         close(client_fd);
//                         break;
//                     } else {
//                         std::cout << "Recv from " << client_fd << ": " << std::string(buf, n) << "\n";
//                         write(client_fd, buf, n); // 回显
//                     }
//                 }
//             }
//         }
//     }

//     close(listen_fd);
//     return 0;
// }

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define MAX_EVENTS 10
#define BUF_SIZE 1024
#define PORT 8888

// 将 fd 设置为非阻塞
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    setNonBlocking(listen_fd);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_fd, SOMAXCONN);

    int epoll_fd = epoll_create1(0);

    epoll_event ev{}, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    char buf[BUF_SIZE];

    std::cout << "Server started on port " << PORT << "\n";
    // 存储客户端的 IP 地址
    char client_ip[INET_ADDRSTRLEN];
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listen_fd) {
                // 有新连接到来
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
                if (conn_fd < 0) {
                    perror("accept");
                    continue;
                }
                setNonBlocking(conn_fd);

                ev.events = EPOLLIN | EPOLLET; // 边缘触发
                ev.data.fd = conn_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
                // 获取客户端的 IP 地址
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

                std::cout << "Accepted new connection from IP: " << client_ip << "\n";

            } else {
                // 客户端有数据可读
                int client_fd = events[i].data.fd;
                while (true) {
                    int n = read(client_fd, buf, BUF_SIZE);
                    if (n == 0) {
                        std::cout << "Client disconnected from IP: " << client_ip << "\n";
                        close(client_fd);
                        break;
                    } else if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // 没有更多数据
                        perror("read error");
                        close(client_fd);
                        break;
                    } else {
                        std::cout << "Recv from " << client_ip << ": " << std::string(buf, n) << "\n";

                        // 使用 std::cin 获取自定义回复内容
                        std::cout << "Enter response for client IP " << client_ip << ": ";
                        std::string response;
                        std::getline(std::cin, response);

                        // 将自定义回复发送给客户端
                        write(client_fd, response.c_str(), response.size());
                    }
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}