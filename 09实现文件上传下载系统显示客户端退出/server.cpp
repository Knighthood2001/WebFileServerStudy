#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <arpa/inet.h>  //用于将 IP 地址从二进制格式（in_addr 或 in6_addr）转换为文本字符串格式
#include "threadpool.h"

constexpr int PORT = 8888;
constexpr int MAX_EVENTS = 1000;
constexpr int BUFFER_SIZE = 1024;

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 处理上传和下载请求
void handleClient(int clientFd) {

    //获取客户端的IP地址和端口号
    sockaddr_in peerAddr{};
    socklen_t peerLen = sizeof(peerAddr);
    char ipStr[INET_ADDRSTRLEN] = {0};
    int port = 0;

    if (getpeername(clientFd, (sockaddr*)&peerAddr, &peerLen) == 0) {
        inet_ntop(AF_INET, &peerAddr.sin_addr, ipStr, sizeof(ipStr));
        port = ntohs(peerAddr.sin_port);
        // std::cout << "客户端连接: " << ipStr << ":" << port << std::endl;
    }


    char buffer[BUFFER_SIZE];
    std::string commandLine;

    // 接收命令（UPLOAD filename\n 或 DOWNLOAD filename\n）
    while (true) {
        // 接收单个字符
        ssize_t n = recv(clientFd, buffer, 1, 0);
        if (n <= 0) {
            // 如果接收失败或连接关闭，关闭客户端连接并返回
            close(clientFd);
            return;
        }
        // 如果接收到换行符，跳出循环
        if (buffer[0] == '\n') break;
        // 拼接接收到的命令
        commandLine += buffer[0];
    }

    // 将接收到的命令和文件名解析出来
    std::istringstream iss(commandLine);
    std::string command, filename;
    iss >> command;
    if (command == "EXIT"){
        std::cout << "断开连接: " << ipStr << ":" << port << std::endl;
    }else if (command == "UPLOAD") {// 如果是上传命令
        iss >> filename;
        // 获取文件名（不包含路径）
        std::string basename = filename.substr(filename.find_last_of("/\\") + 1);
        // 构造文件的完整路径
        std::string fullpath = "filedir/" + basename;
        // 打开文件准备写入
        std::ofstream outfile(fullpath, std::ios::binary);
        if (!outfile.is_open()) {
            // 如果文件打开失败，输出错误信息并关闭连接
            std::cerr << "无法创建文件: " << fullpath << std::endl;
            close(clientFd);
            return;
        }
        // 接收文件数据并写入文件
        while (true) {
            ssize_t bytes = recv(clientFd, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;
            outfile.write(buffer, bytes);
        }
        outfile.close();
        // 输出上传完成信息
        std::cout << "上传完成: " << basename << std::endl;

    // 如果是下载命令
    } else if (command == "DOWNLOAD") {
        iss >> filename;
        // 获取文件名（不包含路径）
        std::string basename = filename.substr(filename.find_last_of("/\\") + 1);
        // 构造文件的完整路径
        std::string fullpath = "filedir/" + basename;
        // 打开文件准备读取
        std::ifstream infile(fullpath, std::ios::binary);
        if (!infile.is_open()) {
            // 如果文件打开失败，发送错误信息并关闭连接
            std::string msg = "ERROR 文件不存在\n";
            send(clientFd, msg.c_str(), msg.size(), 0);
            close(clientFd);
            return;
        }

        // 获取文件大小
        infile.seekg(0, std::ios::end);
        size_t filesize = infile.tellg();
        infile.seekg(0);
        // 构造文件头信息
        std::string header = "OK " + std::to_string(filesize) + "\n";
        send(clientFd, header.c_str(), header.size(), 0);

        // 发送文件数据
        while (infile.read(buffer, BUFFER_SIZE) || infile.gcount() > 0) {
            send(clientFd, buffer, infile.gcount(), 0);
        }
        infile.close();
        // 输出下载完成信息
        std::cout << "下载完成: " << basename << std::endl;
    }
    // 关闭客户端连接
    close(clientFd);
}

int main() {
    // 创建套接字
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "Socket 创建失败\n";
        return 1;
    }

    // 设置套接字选项，允许地址重用
    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 设置套接字为非阻塞模式
    setNonBlocking(serverSock);

    // 配置服务器地址和端口
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字到指定地址和端口
    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "绑定失败\n";
        return 1;
    }

    // 使套接字进入监听状态
    if (listen(serverSock, SOMAXCONN) < 0) {
        std::cerr << "监听失败\n";
        return 1;
    }

    // 创建 epoll 实例
    int epollFd = epoll_create1(0);
    if (epollFd < 0) {
        std::cerr << "创建 epoll 失败\n";
        return 1;
    }

    // 向 epoll 添加监听服务器套接字的事件
    epoll_event ev{};
    ev.data.fd = serverSock;
    ev.events = EPOLLIN;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSock, &ev);

    // 初始化事件数组和线程池
    epoll_event events[MAX_EVENTS];
    ThreadPool pool(std::thread::hardware_concurrency());

    // 输出服务器启动信息
    std::cout << "服务端启动，端口 " << PORT << "...\n";

    // 事件循环
    while (true) {
        // 等待事件发生
        int n = epoll_wait(epollFd, events, MAX_EVENTS, -1);  //epoll_wait 会阻塞直到有事件发生。
        for (int i = 0; i < n; ++i) {
            // 如果是服务器套接字事件
            if (events[i].data.fd == serverSock) {                // 有新连接
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                int clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);  //有客户端连接进来，使用 accept() 拿到新 socket。
                //下面四行是用来打印客户端的 IP 和端口，方便调试。
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
                int clientPort = ntohs(clientAddr.sin_port);
                // std::cout << "新连接来自: " << ipStr << ":" << clientPort << std::endl;

                if (clientSock >= 0) {
                    setNonBlocking(clientSock);  //设置客户端 socket 为非阻塞，因为 epoll 边缘触发要求非阻塞行为。否则可能导致死锁或事件错过。
                    epoll_event clientEvent{};  //构造一个 epoll_event 结构体。
                    clientEvent.data.fd = clientSock;
                    clientEvent.events = EPOLLIN | EPOLLET;  // EPOLLIN: 表示“可读”事件。EPOLLET: 表示边缘触发（只通知一次，除非有新事件到达）。
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSock, &clientEvent);  //把新客户端 fd 加入 epoll 监听列表。
                }
            } else {//如果是其他客户端发送数据了（非监听 socket
                // 将客户端任务交给线程池处理（epoll 边缘触发仅通知一次）
                int clientFd = events[i].data.fd;
                epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);  // 因为是边缘触发（ET），如果不删除，该 fd 后续不会再触发事件了。因此删掉再处理
                pool.enqueue([clientFd]() {  //使用线程池处理客户端任务。 避免阻塞主线程（主线程要负责 epoll_wait）。
                    // handleClient(clientFd) 是处理上传/下载逻辑的函数。
                    handleClient(clientFd);
                });
            }
        }
    }

    // 关闭服务器套接字
    close(serverSock);
    return 0;
}
