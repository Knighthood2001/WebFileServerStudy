#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    std::string filepath;
    std::cout << "请输入要上传的文件路径: ";
    std::getline(std::cin, filepath);

    // 打开文件
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filepath << std::endl;
        return 1;
    }

    // 提取文件名（不含路径）
    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // 连接服务器
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "连接服务器失败" << std::endl;
        return 1;
    }

    // 发送命令：UPLOAD filename\n
    std::string command = "UPLOAD " + filename + "\n";
    send(sock, command.c_str(), command.size(), 0);

    // 发送文件内容
    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {  //从文件中读取sizeof(buffer)（即1024字节）的数据到buffer中。gcount函数返回上一次非格式化输入操作（如read）读取的字符数
        send(sock, buffer, file.gcount(), 0);
    }

    // 关闭文件
    file.close();
    // 关闭套接字
    close(sock);

    std::cout << "文件上传完成: " << filename << std::endl;
    return 0;
}
