
all : udp_server udp_client main

udp_server : udp_server.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11

udp_client : udp_client.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11

main : main.cc ikcp.cpp kcp.cpp kcpmanager.cpp
	g++ $^ -o $@ -std=c++11 -g -lutils -llog -lpthread

.PHONY :
	clean udp_server udp_client

clean :
	rm -rf udp_server udp_client