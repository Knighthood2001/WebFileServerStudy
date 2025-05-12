# 默认目标
all: server client

# 编译 server 目标
server: server.cpp threadpool.cpp
	g++ server.cpp threadpool.cpp -o server -pthread

# 编译 client 目标
client: client.cpp
	g++ client.cpp -o client

# 清理目标
clean:
	rm -f server client
