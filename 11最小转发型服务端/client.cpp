#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
constexpr int BUFFER_SIZE = 4096;

void recvLoop(int sockfd) {
    enum State { IDLE, RECV_FILE };
    State state = IDLE;

    char buffer[BUFFER_SIZE];
    std::ofstream outfile;
    std::string filename;
    size_t filesize = 0, received = 0;
    std::string leftover;

    while (true) {
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            // 连接断开
            std::cerr << "连接断开\n";
            break;
        }

        // 把新数据追加到未处理的缓冲中
        leftover.append(buffer, n);

        // 处理接收数据
        while (true) {
            if (state == IDLE) {
                // 查找换行，尝试解析通知头
                size_t pos = leftover.find('\n');
                if (pos == std::string::npos) break;

                std::string line = leftover.substr(0, pos);
                leftover.erase(0, pos + 1);

                if (line.rfind("INCOMING ", 0) == 0) {
                    // 格式: INCOMING filename filesize
                    std::istringstream iss(line);
                    std::string cmd;
                    iss >> cmd >> filename >> filesize;

                    std::cout << "[通知] 即将接收文件: " << filename << " (" << filesize << "字节)\n";

                    outfile.open(filename, std::ios::binary);
                    if (!outfile) {
                        // 无法创建文件
                        std::cerr << "无法创建文件: " << filename << "\n";
                        break;
                    }

                    received = 0;
                    state = RECV_FILE;
                }
            }

            if (state == RECV_FILE) {
                // 剩下的数据可能是文件内容
                size_t remain = filesize - received;
                size_t toWrite = std::min(remain, leftover.size());

                outfile.write(leftover.data(), toWrite);
                received += toWrite;
                leftover.erase(0, toWrite);

                if (received >= filesize) {
                    // 文件接收完成
                    std::cout << "[完成] 接收文件: " << filename << "\n";
                    std::cout << "\n发送文件格式： 目标名称 文件路径\n> ";
                    std::cout.flush();
                    outfile.close();
                    state = IDLE;
                } else {
                    // 文件未接收完，等更多数据
                    break;
                }
            } else {
                break;
            }
        }
    }
}


/**
 * @brief 主函数
 *
 * 此函数负责初始化网络连接，接收用户输入，并循环发送文件到服务器。
 *
 * @return int 0 表示程序正常退出，1 表示连接失败
 */
int main() {
    std::string serverIp = "127.0.0.1";
    int port = 9999;

    // 创建套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // 初始化服务器地址结构体
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, serverIp.c_str(), &serv_addr.sin_addr);

    // 连接到服务器
    if (connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("连接失败");
        return 1;
    }

    // 输入用户名并发送给服务器
    std::string name;
    std::cout << "输入用户名: ";
    std::getline(std::cin, name);
    std::string reg = "NAME " + name + "\n";
    send(sockfd, reg.c_str(), reg.size(), 0);

    // 创建接收线程
    std::thread receiver(recvLoop, sockfd);

    // 循环发送文件
    while (true) {
        std::cout << "\n发送文件格式： 目标名称 文件路径\n> ";
        std::string target, filepath;
        std::cin >> target >> filepath;
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            std::cerr << "无法打开文件\n";
            continue;
        }

        // 获取文件大小
        in.seekg(0, std::ios::end); //将文件指针移动到文件末尾，获取当前位置作为文件大小
        size_t size = in.tellg(); // 获取文件大小
        in.seekg(0);// 将文件指针重新移动到文件的开头，以便读取内容

        // 构造文件头信息
        std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
        std::string header = "SEND " + target + "\n" + filename + "\n" + std::to_string(size) + "\n";
        send(sockfd, header.c_str(), header.size(), 0);

        // 发送文件内容
        char buffer[BUFFER_SIZE];
        size_t sent = 0;
        while (sent < size) {
            in.read(buffer, sizeof(buffer));
            std::streamsize n = in.gcount();
            send(sockfd, buffer, n, 0);
            sent += n;
        }

        // 输出发送状态
        std::cout << "已发送 " << filename << " (" << size << " 字节)\n";
    }

    // 等待接收线程结束
    receiver.join();
    close(sockfd);
    return 0;
}
