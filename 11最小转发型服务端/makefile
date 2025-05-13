all: server a_client b_client

server: server.cpp
	g++ server.cpp -o server

a_client: client.cpp
	mkdir -p a
	g++ client.cpp -o a/client -pthread

b_client: client.cpp
	mkdir -p b
	g++ client.cpp -o b/client -pthread

clean:
	rm -f server a/client b/client
