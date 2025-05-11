all: server_nonblock client

server: server_nonblock.cpp
	g++ server_nonblock.cpp -o server_nonblock

client: client.cpp
	g++ client.cpp -o client

run: all
	./server_nonblock &
	sleep 1
	./client

clean:
	rm -f server_nonblock client
