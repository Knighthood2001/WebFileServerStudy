all: epoll_server client

server: epoll_server.cpp
	g++ epoll_server.cpp -o epoll_server

client: client.cpp
	g++ client.cpp -o client

run: all
	./epoll_server &
	sleep 1
	./client

clean:
	rm -f epoll_server client
