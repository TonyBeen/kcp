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

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    UdpClientInfo *cliInfo = (UdpClientInfo *)user;
    assert(cliInfo->fd > 0);

    return ::sendto(cliInfo->fd, buf, len, 0, (sockaddr *)&cliInfo->addr, cliInfo->size);
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

    ikcp_wndsize(gKcpClient, 8, 8);
    ikcp_nodelay(gKcpClient, 0, 20, 2, 1);

    const char *Hello = "Hello";
    size_t len = strlen(Hello);
    int sendSize = ::sendto(udpSock, Hello, len, 0, (sockaddr *)&cliInfo.addr, cliInfo.size);
    assert(sendSize == len);
    LOGI("success connect to %s:%d", UDP_SERVER_IP, UDP_SERVER_PORT);

    char buf[256] = {0};
    char recvBuf[256] = {0};
    const char *str[] = {"hello world", "hello kcp"};
    int index = 0;
    while (1)
    {
        sleep(1);
        ikcp_update(gKcpClient, iclock());
        // printf("input:\n");
        // int readSize = read(STDIN_FILENO, buf, 16);
        // LOGI("read from stdin: [%s,%d]", buf, readSize);
        // if (readSize <= 0) {
        //     perror("read error");
        //     break;
        // }

        const char *temp = str[index % (sizeof(str) / sizeof(char *))];
        assert(ikcp_send(gKcpClient, temp, strlen(temp)) == 0);
        index++;

rerecv:
        int recvSize = ::recvfrom(udpSock, recvBuf, sizeof(recvBuf), 0, (sockaddr *)&cliInfo.addr, &cliInfo.size);
        if (recvSize < 0) {
            if (errno != EAGAIN) {
                perror("recvfrom error");
                break;
            }
            ikcp_update(gKcpClient, iclock());
            goto rerecv;
        }

        if (recvSize == 0) {
            LOGI("server quit\n");
            break;
        }

        ikcp_input(gKcpClient, recvBuf, recvSize);
        ikcp_update(gKcpClient, iclock());
        eular::String8 log;
        for (;;) {
            memset(buf, 0, sizeof(buf));
            recvSize = ikcp_recv(gKcpClient, buf, sizeof(buf));
            if (recvSize < 0) {
                break;
            }
            for (int i = 0; i < recvSize; ++i) {
                log.appendFormat("0x%02x ", buf[i]);
            }
            LOGD("ikcp_recv: [%s,%d]\n", log.c_str(), recvSize);
            LOGD("ikcp_recv: %s", buf);
            log.clear();
            memset(buf, 0, sizeof(buf));
        }

        memset(buf, 0, sizeof(buf));
        memset(recvBuf, 0, sizeof(recvBuf));
    }

    close(udpSock);
    ikcp_release(gKcpClient);
    return 0;
}
