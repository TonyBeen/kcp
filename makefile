
all : udp_server udp_client

udp_server : udp_server.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11 -lutils -llog

udp_client : udp_client.cc ikcp.cpp
	g++ $^ -o $@ -std=c++11 -lutils -llog

.PHONY :
	clean udp_server udp_client

clean :
	rm -rf udp_server udp_client