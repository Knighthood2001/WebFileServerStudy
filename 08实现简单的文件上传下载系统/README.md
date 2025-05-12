上面的代码中当客户端exit后，没有与服务端通信，导致服务端不知道客户端退出。

当前服务端的命令处理逻辑是：

```
UPLOAD filename\n
DOWNLOAD filename\n
```

而你现在希望添加 `exit\n`，但这条命令格式不同，这 **确实让协议变得“不统一”，不优雅”。**

---

### ✅ 更优雅的方案：**统一协议格式**

我们可以把所有命令都统一成这个格式：

```
COMMAND [args...]\n
```

比如：

| 功能    | 协议命令                  |
| ----- | --------------------- |
| 上传文件  | `UPLOAD filename\n`   |
| 下载文件  | `DOWNLOAD filename\n` |
| 客户端退出 | `EXIT\n`              |

这样 `EXIT` 也是一个合法命令，服务端也能像解析其他命令一样解析它。

---

### 🔧 服务端代码（解析命令部分）：

```cpp
std::istringstream iss(commandLine);
std::string command, filename;
iss >> command;

if (command == "UPLOAD") {
    iss >> filename;
    // ... 处理上传
} else if (command == "DOWNLOAD") {
    iss >> filename;
    // ... 处理下载
} else if (command == "EXIT") {
    // 客户端主动断开
    getpeername(...) 打印IP和端口;
    close(clientFd);
    return;
} else {
    std::cerr << "未知命令: " << command << std::endl;
    close(clientFd);
    return;
}
```

---

### 🔧 客户端退出部分改为发送 `EXIT\n`：

```cpp
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
```

---

### ✅ 优点

* 协议风格统一，容易扩展（比如以后加 `PING`, `LIST`, `RENAME` 等命令）。
* 服务端处理逻辑清晰，维护方便。
* 客户端行为明确，也更标准。

你希望我帮你抽象成一个小的 `CommandParser` 类用于后续扩展吗？
