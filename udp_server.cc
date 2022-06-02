/*************************************************************************
    > File Name: udp_server.cc
    > Author: hsz
    > Brief:
    > Created Time: Thu 24 Feb 2022 03:34:46 PM CST
 ************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>

#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "ikcp.h"   // ikcp定义inline会使标准库报错

using namespace std;

#define KCP_KEY     0x1024
#define LOCAL_IP    "172.25.12.215"
#define UDP_PORT    8500

ikcpcb *gKcpServer = nullptr;

const char *resp[] = { "OK", "ERROR" };


struct UdpClientInfo
{
   int fd;
   sockaddr_in addr;
   socklen_t size;
};

uint32_t iclock()
{
    struct timeval time = {0, 0};
	gettimeofday(&time, NULL);
    return (uint32_t)(time.tv_sec * 1000 + time.tv_usec / 1000);
}

int create_udp_server()
{
    int udpSock = ::socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpSock > 0);

    int reUse = 1;
    int ret = setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &reUse, sizeof(reUse));
    struct timeval tv_out;
    tv_out.tv_sec = 0;
    tv_out.tv_usec = 20 * 1000;
    ret = setsockopt(udpSock, SOL_SOCKET, SO_SNDTIMEO, &tv_out, sizeof(tv_out));
    ret = setsockopt(udpSock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    int flag = fcntl(udpSock, F_GETFL);
    fcntl(udpSock, F_SETFL, flag | O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(LOCAL_IP);
    addr.sin_port = htons(UDP_PORT);

    ::bind(udpSock, (sockaddr *)&addr, sizeof(addr));

    return udpSock;
}

std::string hexdump(const char *buf, size_t len)
{
    std::string log;
    char temp[128] = {0};
    for (int i = 0; i < len; i++) {
        if (i % 24 == 0) {
            log.append("\n\t\t");
        }
        snprintf(temp, sizeof(temp), "0x%02x ", (uint8_t)buf[i]);
        log.append(temp);
    }

    log.append("\n");
    return log;
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    UdpClientInfo *cliInfo = (UdpClientInfo *)user;
    assert(cliInfo->fd > 0);

    std::string log = hexdump(buf, len);
    printf("%s() %s\n", __func__, log.c_str());
    return ::sendto(cliInfo->fd, buf, len, 0, (sockaddr *)&cliInfo->addr, cliInfo->size);
}

void onTimerOut()
{
    ikcp_update(gKcpServer, iclock());
}

void log(const char *buf, ikcpcb *kcp, void *user)
{
    write(STDOUT_FILENO, buf, strlen(buf));
}

int main(int argc, char **argv)
{
    int udpSock = create_udp_server();
    UdpClientInfo cliInfo;
    cliInfo.fd = udpSock;
    cliInfo.size = sizeof(sockaddr_in); 

    gKcpServer = ikcp_create(KCP_KEY, (void *)&cliInfo);
    assert(gKcpServer);
    gKcpServer->output = udp_output;
    //gKcpServer->writelog = log;

    int interval = 20;
    ikcp_wndsize(gKcpServer, 8, 8);
    ikcp_nodelay(gKcpServer, 0, interval, 2, 1);

    int efd = epoll_create(8);
    epoll_event events[8];
    epoll_event ee;
    ee.data.fd = udpSock;
    ee.events = EPOLLET | EPOLLIN;

    epoll_ctl(efd, EPOLL_CTL_ADD, udpSock, &ee);

    printf("udp server listen on %s:%d\n", LOCAL_IP, UDP_PORT);
    char buf[1024] = {0};
    while (1) {
        int nev = epoll_wait(efd, events, sizeof(events) / sizeof(epoll_event), interval);
        if (nev < 0) {
            perror("epoll_wait");
            break;
        }
        if (nev == 0) {
            ikcp_update(gKcpServer, iclock());
        }
        for (int i = 0; i < nev; ++i) {
            epoll_event &ev = events[i];
            int sock = ev.data.fd;
            int recvSize = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr *)&cliInfo.addr, &cliInfo.size);
            if (recvSize <= 0) {
                if (errno != EAGAIN) {
                    printf("recvfrom error. [%d,%s]\n", errno, strerror(errno));
                    return 0;
                }
                continue;
            }
            std::string log = hexdump(buf, recvSize);
            printf("recvfrom [%s:%d]: %d %s\n", inet_ntoa(cliInfo.addr.sin_addr), ntohs(cliInfo.addr.sin_port), recvSize, log.c_str());

            ikcp_input(gKcpServer, buf, recvSize);
            memset(buf, 0, sizeof(buf));
            recvSize = ikcp_recv(gKcpServer, buf, sizeof(buf));
            if (recvSize < 0) {
                printf("recvSize %d\n", recvSize);
                continue;
            }
            printf("content: %s\n", buf);
            ikcp_send(gKcpServer, buf, recvSize);
        }
    }

    return 0;
}
