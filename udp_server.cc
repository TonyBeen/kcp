/*************************************************************************
    > File Name: udp_server.cc
    > Author: hsz
    > Brief:
    > Created Time: Thu 24 Feb 2022 03:34:46 PM CST
 ************************************************************************/

#include <utils/timer.h>
#include <utils/string8.h>
#include <log/log.h>

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

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(LOCAL_IP);
    addr.sin_port = htons(UDP_PORT);

    ::bind(udpSock, (sockaddr *)&addr, sizeof(addr));

    return udpSock;
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    UdpClientInfo *cliInfo = (UdpClientInfo *)user;
    assert(cliInfo->fd > 0);
    eular::String8 log;
    for (int i = 0; i < len; i++) {
        log.appendFormat("0x%02x ", (uint8_t)buf[i]);
    }
    LOGD("%s() %s", __func__, log.c_str());

    return ::sendto(cliInfo->fd, buf, len, 0, (sockaddr *)&cliInfo->addr, cliInfo->size);
}

void onTimerOut()
{
    ikcp_update(gKcpServer, iclock());
}

int main(int argc, char **argv)
{
    eular::InitLog(eular::LogLevel::DEBUG);
    eular::addOutputNode(eular::LogWrite::FILEOUT);

    eular::TimerManager timerManager;
    timerManager.StartTimer();
    auto timer_id = timerManager.addTimer(20, onTimerOut, 20);
    LOGI("%s() timer id: %llu", __func__, timer_id);

    int udpSock = create_udp_server();
    UdpClientInfo cliInfo;
    cliInfo.fd = udpSock;
    cliInfo.size = sizeof(sockaddr_in); 

    gKcpServer = ikcp_create(KCP_KEY, (void *)&cliInfo);
    assert(gKcpServer);
    gKcpServer->output = udp_output;

    ikcp_wndsize(gKcpServer, 8, 8);
    ikcp_nodelay(gKcpServer, 0, 20, 2, 1);

    LOGI("udp server listen on %s:%d\n", LOCAL_IP, UDP_PORT);
    char buf[1024] = {0};
    while (1)
    {
        int recvSize = ::recvfrom(udpSock, buf, sizeof(buf), 0, (sockaddr *)&cliInfo.addr, &cliInfo.size);
        if (recvSize <= 0) {
            if (errno != EAGAIN) {
                LOGE("recvfrom error. [%d,%s]", errno, strerror(errno));
                return 0;
            }
            continue;
        }

        // if (strcmp(buf, "Hello") == 0) {
        //     LOGI("client [%s:%d] connect", inet_ntoa(cliInfo.addr.sin_addr), ntohs(cliInfo.addr.sin_port));
        //     continue;
        // }
        eular::String8 log;
        for (int i = 0; i < recvSize; i++) {
            log.appendFormat("0x%02x ", (uint8_t)buf[i]);
        }
        LOGD("%s", log.c_str());
        LOGI("[%s:%d]: [%s:%d]", inet_ntoa(cliInfo.addr.sin_addr), ntohs(cliInfo.addr.sin_port), buf, recvSize);
        ikcp_input(gKcpServer, buf, recvSize);
        
        log.clear();
        eular::String8 retStr;
        bool flag = false;
        for (;;) {
            memset(buf, 0, sizeof(buf));
            int ret = ikcp_recv(gKcpServer, buf, sizeof(buf));
            if (ret < 0) {
                LOGD("break ret = %d", ret);
                break;
            }

            flag = true;
            for (int i = 0; i < ret; i++) {
                log.appendFormat("0x%02x ", buf[i]);
            }
            LOGD("ikcp_recv: [%s,%d]\n", log.c_str(), ret);
            retStr += buf;
            LOGD("ikcp_recv: %s", buf);
            log.clear();
        }
        
        const char *response = flag ? resp[0] : resp[1];
        int ret = ikcp_send(gKcpServer, retStr.c_str(), retStr.length());
        memset(buf, 0, sizeof(buf));
    }

    return 0;
}
