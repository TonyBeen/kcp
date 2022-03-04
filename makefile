udp_server : udp_server.cc ikcp.c
	g++ ikcp.c udp_server.cc -o udp_server -std=c++11 -lutils -llog

udp_client : udp_client.cc ikcp.c
	g++ ikcp.c udp_client.cc -o udp_client -std=c++11 -lutils -llog

.PHONY :
	clean

clean :
	rm -rf udp_server udp_client