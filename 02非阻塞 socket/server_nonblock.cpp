//
// Created by wu on 25-5-10.
//
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }

    // 设置为非阻塞
    setNonBlocking(server_fd);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    std::cout << "Server listening on port 8888 (non-blocking)..." << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有新连接，稍后再试
                usleep(100000); // 睡100ms，防止CPU占满
                continue;
            } else {
                perror("accept error");
                break;
            }
        }

        std::cout << "Accepted new client." << std::endl;

        // 设置 client_fd 为非阻塞
        setNonBlocking(client_fd);

        // 尝试读取数据（非阻塞）
        char buffer[1024];
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cout << "No data from client yet." << std::endl;
            } else {
                perror("recv error");
            }
        } else if (bytes == 0) {
            std::cout << "Client disconnected." << std::endl;
            close(client_fd);
        } else {
            buffer[bytes] = '\0';
            std::cout << "Received: " << buffer << std::endl;

            // 回复
            const char* reply = "Hi from non-blocking server!";
            send(client_fd, reply, strlen(reply), 0);
        }

        close(client_fd); // 简单处理，只处理一次交互
    }

    close(server_fd);
    return 0;
}
