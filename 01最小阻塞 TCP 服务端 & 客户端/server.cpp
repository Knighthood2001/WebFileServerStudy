//
// Created by wu on 25-5-10.
//
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    // 创建 socket（IPv4，TCP）
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }

    // 设置地址结构
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888); // 端口8888
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有 IP

    // 绑定地址
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    // 开始监听
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "Server listening on port 8888..." << std::endl;

    // 接受客户端连接
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
        return 1;
    }

    std::cout << "Client connected!" << std::endl;

    // 接收数据
    char buffer[1024];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "Received: " << buffer << std::endl;

        // 回复客户端
        const char* reply = "Hello from server!";
        send(client_fd, reply, strlen(reply), 0);
    }

    // 关闭连接
    close(client_fd);
    close(server_fd);
    return 0;
}
