#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#define SERVER_IP "43.143.168.49"
#define PORT 8888
#define BUFFER_SIZE 1024

// 读取一行直到 \n
/**
 * @brief 从指定套接字接收一行数据
 *
 * 从指定的套接字中接收一行数据，直到遇到换行符（\n）或接收失败。
 *
 * @param sock 套接字描述符
 * @return 接收到的行数据
 */
std::string recvLine(int sock) {
    // 定义一个字符变量ch
    char ch;
    // 定义一个字符串变量line用于存储接收到的行数据
    std::string line;
    // 循环接收数据直到遇到换行符或接收失败
    while (recv(sock, &ch, 1, 0) > 0) {
        // 如果接收到换行符，则跳出循环
        if (ch == '\n') break;
        // 将接收到的字符添加到line中
        line += ch;
    }
    // 返回接收到的行数据
    return line;
}

int main() {
    while (true){
        std::cout << "\n请输入命令（upload/download 文件名 或 exit）: ";
        std::string input;
        std::getline(std::cin, input);
        if (input == "exit") {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(PORT);
            inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
        
            if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                std::string command = "EXIT\n";
                send(sock, command.c_str(), command.size(), 0);
            }
            close(sock);
            break;
        }
        std::istringstream iss(input);
        std::string cmd, filename;
        iss >> cmd >> filename;
        if (cmd != "upload" && cmd != "download") {
            std::cerr << "无效的命令，请输入 'upload' 或 'download'" << std::endl;
            continue;
        }
        // 提取文件名（不含路径）最好不带路径
        std::string only_filename = filename.substr(filename.find_last_of("/\\") + 1);

        // 创建连接
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "连接服务器失败" << std::endl;
            continue;
        }
        if (cmd == "upload"){//上传文件
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "无法打开文件: " << filename << std::endl;
                close(sock);
                continue;
            }
        
            // 获取文件大小
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            std::cout << "开始上传文件: " << only_filename << " (总大小: " << file_size << " 字节)" << std::endl;
        
            // 发送命令：UPLOAD filename\n
            std::string command = "UPLOAD " + only_filename + "\n";
            send(sock, command.c_str(), command.size(), 0);
        
            // 发送文件大小
            std::string file_size_str = std::to_string(file_size) + "\n";
            send(sock, file_size_str.c_str(), file_size_str.size(), 0);
        
            // 发送文件内容
            char buffer[BUFFER_SIZE];
            size_t sent = 0;
            while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
                ssize_t bytes_sent = send(sock, buffer, file.gcount(), 0);
                if (bytes_sent < 0) {
                    std::cerr << "发送文件内容时出错" << std::endl;
                    close(sock);
                    break;
                }
                sent += bytes_sent;
                
                // 每发送1MB显示一次进度，或者在发送完成时显示
                if (sent % (1024 * 1024) == 0 || sent == file_size) {
                    float progress = (float)sent / file_size * 100;
                    std::cout << "已上传: " << sent << "/" << file_size 
                             << " 字节 (" << std::fixed << std::setprecision(2) << progress << "%)" << std::endl;
                }
            }
        
            file.close();
            if (sent < file_size) {
                std::cerr << "上传未完成，实际发送: " << sent << "/" << file_size << " 字节" << std::endl;
            } else {
                std::cout << "上传完成: " << only_filename << " (总大小: " << sent << " 字节)" << std::endl;
            }
        }else if (cmd =="download"){
            // 发送下载命令
            std::string command = "DOWNLOAD " + only_filename + "\n";
            send(sock, command.c_str(), command.size(), 0);

            std::string response = recvLine(sock);  // 接收服务器的响应
            if (response.substr(0, 5) == "ERROR") {
                std::cerr << "服务端错误: " << response << std::endl;
                close(sock);
                continue;
            }
            //OK <filesize>\n 服务端响应格式，解析文件大小,比如OK 1048576\n
            size_t filesize = 0;
            std::istringstream ss(response);
            std::string status;
            ss >> status >> filesize;

            std::cout << "开始下载文件: " << only_filename << " (总大小: " << filesize << " 字节)" << std::endl;
           
            std::ofstream outfile(only_filename, std::ios::binary);
            char buffer[BUFFER_SIZE];
            size_t received = 0;
            while (received < filesize) {
                ssize_t len = recv(sock, buffer, BUFFER_SIZE, 0);
                if (len <= 0) break;
                outfile.write(buffer, len);
                received += len;

                // 每接收1MB数据显示一次进度，或者在接收完成时显示
                if (received % (1024 * 1024) == 0 || received == filesize) {
                    float progress = (float)received / filesize * 100;
                    std::cout << "已下载: " << received << "/" << filesize 
                                << " 字节 (" << std::fixed << std::setprecision(2) << progress << "%)" << std::endl;
                }
            }

            outfile.close();
            if (received < filesize) {
                std::cerr << "下载未完成，实际接收: " << received << "/" << filesize << " 字节" << std::endl;
            } else {
                std::cout << "下载完成: " << only_filename << " (总大小: " << received << " 字节)" << std::endl;
            }
        }

        close(sock);
    }



    return 0;
}
