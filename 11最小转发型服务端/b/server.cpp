#include <iostream>
#include <unordered_map>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <mutex>

constexpr int PORT = 9999;
constexpr int MAX_EVENTS = 1024;
constexpr int BUFFER_SIZE = 4096;

std::unordered_map<std::string, int> nameToFd;
std::unordered_map<int, std::string> fdToName;
std::mutex mapMutex;

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * @brief 关闭连接
 *
 * 该函数用于关闭一个已经建立的连接，并将其从epoll事件监控列表中移除。
 *
 * @param fd 要关闭的连接的文件描述符
 * @param epollFd epoll事件监控列表的文件描述符
 */
void closeConnection(int fd, int epollFd) {
    // 从epoll事件中删除文件描述符fd
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    {
        // 加锁以保护线程安全
        std::lock_guard<std::mutex> lock(mapMutex);

        // 检查fd是否在fdToName映射中存在
        if (fdToName.count(fd)) {
            // 获取fd对应的名称
            std::string name = fdToName[fd];

            // 从nameToFd映射中删除该名称对应的fd
            nameToFd.erase(name);

            // 从fdToName映射中删除该fd
            fdToName.erase(fd);

            // 输出断开连接的客户端名称
            std::cout << "客户端 " << name << " 断开连接\n";
        }
    }

    // 关闭文件描述符fd
    close(fd);
}

/**
 * @brief 从文件描述符中读取一行文本
 *
 * 从指定的文件描述符中读取一行文本，直到遇到换行符（\n）为止。
 *
 * @param fd 文件描述符
 * @param line 用于存储读取到的行的字符串引用
 *
 * @return 如果成功读取到一行文本，则返回 true；否则返回 false
 */
bool readLine(int fd, std::string& line) {
    // 初始化字符变量
    char c;
    // 清空字符串
    line.clear();
    // 循环读取字符直到遇到换行符或读取失败
    while (true) {
        // 从文件描述符fd中读取1个字符到变量c中
        ssize_t n = recv(fd, &c, 1, 0);
        // 如果读取到的字节数小于等于0，则表示读取失败或连接关闭，返回false
        if (n <= 0) return false;
        // 如果读取到的字符是换行符，则跳出循环
        if (c == '\n') break;
        // 将读取到的字符追加到字符串line中
        line += c;
    }
    // 读取成功，返回true
    return true;
}

/**
 * @brief 处理客户端请求
 *
 * 根据传入的文件描述符 fd 和 epoll 文件描述符 epollFd 处理客户端请求
 *
 * @param fd 客户端的文件描述符
 * @param epollFd epoll 的文件描述符
 */
 //handleClient 是在服务器监听到已连接客户端有数据可读事件时被调用的，无论客户端发送的是什么数据（比如客户端名称、文件传输请求等），
 // 只要数据到达服务器并触发了可读事件，handleClient 就会被执行来处理这些数据。

void handleClient(int fd, int epollFd) {
    std::string line;
    // 读取客户端发送的数据行
    if (!readLine(fd, line)) {
        // 如果读取失败，则关闭连接
        closeConnection(fd, epollFd);
        return;
    }

    // 使用istringstream解析数据行
    std::istringstream iss(line);
    std::string cmd, arg;
    iss >> cmd >> arg;

    // 处理"NAME"命令   NAME aaa
    if (cmd == "NAME") {
        std::lock_guard<std::mutex> lock(mapMutex);
        nameToFd[arg] = fd;
        fdToName[fd] = arg;
        // 输出客户端注册信息
        std::cout << "客户端注册为: " << arg << "\n";
    } 

    /*
    处理"SEND"命令
    SEND <目标名aaa>\n
    <文件名>\n
    <文件大小>\n
    <文件数据>
    由于上面已经读取了send 目标名
    */
    else if (cmd == "SEND") {
        std::string filename, sizeLine;
        // 读取文件名和文件大小，这是读取两行数据
        if (!readLine(fd, filename) || !readLine(fd, sizeLine)) {
            std::cerr << "读取文件信息失败\n";
            closeConnection(fd, epollFd);
            return;
        }

        size_t filesize = std::stoull(sizeLine);// 将字符串转换为无符号长整型，表示文件大小
        std::lock_guard<std::mutex> lock(mapMutex);
        // 检查接收方是否在线
        if (!nameToFd.count(arg)) {
            std::string err = "ERROR 接收方 " + arg + " 不在线\n";
            send(fd, err.c_str(), err.size(), 0);
            return;
        }

        int targetFd = nameToFd[arg];  // 获取目标客户端的文件描述符
        std::string notify = "INCOMING " + filename + " " + sizeLine + "\n";
        send(targetFd, notify.c_str(), notify.size(), 0);

        size_t sent = 0;
        char buffer[BUFFER_SIZE];
        while (sent < filesize) {
            // 从客户端接收数据并转发到目标客户端
            ssize_t n = recv(fd, buffer, std::min(static_cast<size_t>(BUFFER_SIZE), filesize - sent), 0);

            if (n <= 0) {
                std::cerr << "接收中断，终止转发\n";
                closeConnection(fd, epollFd);
                return;
            }
            send(targetFd, buffer, n, 0);
            sent += n;
        }

        // 输出转发完成信息
        std::cout << "已转发 " << filename << " (" << filesize << " 字节) 到 " << arg << "\n";
    } 
    // 处理未知命令
    else {
        std::cerr << "未知命令: " << line << "\n";
        closeConnection(fd, epollFd);
    }
}

int main() {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1; // 设置为1，表示允许重用本地地址和端口
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlocking(serverFd);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(serverFd, SOMAXCONN) < 0) {
        perror("listen");
        return 1;
    }
    // 创建epoll实例并添加serverFd到epoll中用于监听新连接
    int epollFd = epoll_create1(0);
    epoll_event ev{.events = EPOLLIN, .data = {.fd = serverFd}};
    epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &ev);  // 添加serverFd到epoll中用于监听新连接

    epoll_event events[MAX_EVENTS];  // 用于存储epoll_wait返回的事件列表
    std::cout << "转发型服务端已启动，监听端口 " << PORT << "\n";

    while (true) {
        // 等待epoll事件发生，并处理它们
        int n = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == serverFd) {  // 处理新的连接请求
                std::cout << "这是新连接 "<< fd <<std::endl;
                sockaddr_in clientAddr{};
                socklen_t len = sizeof(clientAddr);
                int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &len);
                if (clientFd >= 0) {
                    setNonBlocking(clientFd);
                    epoll_event cev{.events = EPOLLIN, .data = {.fd = clientFd}};
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &cev);
                }
            } else {  // 处理已连接客户端的数据接收或关闭事件
                std::cout << "这是已连接客户端发生相关操作 "<< fd<<std::endl;
                handleClient(fd, epollFd);  //fd是某个一连接的客户端的文件描述符，epollFd是epoll实例的文件描述符
            }
        }
    }

    close(serverFd);
    return 0;
}
