#include <iostream>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define BUF_SIZE 1024

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    const char* msg = "Hello from client!";
    send(sock, msg, strlen(msg), 0);

    char buf[BUF_SIZE];
    int n = recv(sock, buf, BUF_SIZE, 0);
    if (n > 0) {
        std::cout << "Received from server: " << std::string(buf, n) << std::endl;
    }

    close(sock);
    return 0;
}
