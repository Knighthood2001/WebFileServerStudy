all: server client

server: server.cpp
	g++ server.cpp -o server

client: client.cpp
	g++ client.cpp -o client

run: all
	./server &
	sleep 1
	./client

clean:
	rm -f server client
