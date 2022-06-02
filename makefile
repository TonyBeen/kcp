
all : udp_server udp_client

udp_server : udp_server.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11

udp_client : udp_client.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11

.PHONY :
	clean udp_server udp_client

clean :
	rm -rf udp_server udp_client