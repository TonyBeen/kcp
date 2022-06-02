/*************************************************************************
    > File Name: udp_client.cc
    > Author: hsz
    > Brief:
    > Created Time: Thu 24 Feb 2022 03:34:54 PM CST
 ************************************************************************/

#include <log/log.h>
#include <utils/string8.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "ikcp.h"   // ikcp定义inline会使标准库报错

using namespace std;

#define LOG_TAG "udp_server"

#define KCP_KEY         0x1024

#define LOCAL_IP        "172.25.12.215"
#define LOCLA_PORT      8600

#define UDP_SERVER_IP   "39.106.218.123"
#define UDP_SERVER_PORT 8500


ikcpcb *gKcpClient = nullptr;

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

int create_udp()
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

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(LOCAL_IP);
    addr.sin_port = htons(LOCLA_PORT);

    ::bind(udpSock, (sockaddr *)&addr, sizeof(addr));

    return udpSock;
}

void hexdump(const char *buf, size_t len)
{
    eular::String8 log;
    for (int i = 0; i < len; i++) {
        if (i % 24 == 0) {
            log.append("\n\t\t");
        }
        log.appendFormat("0x%02x ", (uint8_t)buf[i]);
    }
    LOGD("%s() %s", __func__, log.c_str());
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    UdpClientInfo *cliInfo = (UdpClientInfo *)user;
    assert(cliInfo->fd > 0);

    return ::sendto(cliInfo->fd, buf, len, 0, (sockaddr *)&cliInfo->addr, cliInfo->size);
}

void log(const char *buf, ikcpcb *kcp, void *user)
{
    write(STDOUT_FILENO, buf, strlen(buf));
}

int main(int argc, char **argv)
{
    int udpSock = create_udp();
    UdpClientInfo cliInfo;
    cliInfo.fd = udpSock;
    cliInfo.size = sizeof(sockaddr_in);
    cliInfo.addr.sin_family = AF_INET;
    cliInfo.addr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
    cliInfo.addr.sin_port = htons(UDP_SERVER_PORT);

    gKcpClient = ikcp_create(KCP_KEY, &cliInfo);
    assert(gKcpClient);
    gKcpClient->output = udp_output;
    //gKcpClient->writelog = log;

    int interval = 20;
    ikcp_wndsize(gKcpClient, 8, 8);
    ikcp_nodelay(gKcpClient, 0, interval, 2, 1);

    const char *Hello = "Hello";
    size_t len = strlen(Hello);
    int sendSize = ::sendto(udpSock, Hello, len, 0, (sockaddr *)&cliInfo.addr, cliInfo.size);
    assert(sendSize == len);
    LOGI("success connect to %s:%d", UDP_SERVER_IP, UDP_SERVER_PORT);

    char buf[256] = {0};
    char recvBuf[256] = {0};
    const char *str[] = {"hello world", "hello kcp"};
    int index = 0;
    
    int efd = epoll_create(8);
    epoll_event events[8];
    epoll_event ee;
    ee.data.fd = udpSock;
    ee.events = EPOLLET | EPOLLIN;

    epoll_ctl(efd, EPOLL_CTL_ADD, udpSock, &ee);
    bool first = true;

    while (true) {
        int nev = epoll_wait(efd, events, sizeof(events) / sizeof(epoll_event), interval);
        if (nev < 0) {
            perror("epoll_wait");
            break;
        }
        if (nev == 0) {
            ikcp_update(gKcpClient, iclock());
            if (first) {
                ikcp_send(gKcpClient, str[0], strlen(str[0]));
                first = false;
            }
        }
        for (int i = 0; i < nev; ++i) {
            epoll_event &ev = events[i];
            int sock = ev.data.fd;
            int recvSize = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr *)&cliInfo.addr, &cliInfo.size);
            if (recvSize <= 0) {
                if (errno != EAGAIN) {
                    LOGE("recvfrom error. [%d,%s]", errno, strerror(errno));
                    return 0;
                }
                continue;
            }
            eular::String8 log;
            for (int i = 0; i < recvSize; i++) {
                if (i % 24 == 0) {
                    log.append("\n\t\t");
                }
                log.appendFormat("0x%02x ", (uint8_t)buf[i]);
            }
            LOGI("recvfrom [%s:%d]: %d %s", inet_ntoa(cliInfo.addr.sin_addr), ntohs(cliInfo.addr.sin_port), recvSize, log.c_str());

            ikcp_input(gKcpClient, buf, recvSize);
            memset(buf, 0, sizeof(buf));
            recvSize = ikcp_recv(gKcpClient, buf, sizeof(buf));
            if (recvSize < 0) {
                LOGE("recvSize %d", recvSize);
                return 0;
            }
            LOGI("content: %s", buf);
            ikcp_send(gKcpClient, buf, recvSize);
            index++;
        }
        if (index == 4) {
            break;
        }
    }

    close(udpSock);
    ikcp_release(gKcpClient);
    return 0;
}
