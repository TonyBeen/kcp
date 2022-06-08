/*************************************************************************
    > File Name: kcp.h
    > Author: hsz
    > Brief:
    > Created Time: Tue 07 Jun 2022 10:09:03 AM CST
 ************************************************************************/

#ifndef __EULAR_KCP_H__
#define __EULAR_KCP_H__

#include "ikcp.h"
#include <utils/Buffer.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <functional>
#include <string>
#include <memory>

#ifdef __linux__
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(WIN32) || defined(_WIN32) || defined(WIN64)
#include <winsock2.h>
#endif

namespace eular {
class Kcp : public std::enable_shared_from_this<Kcp>
{
    friend class KcpManager;
public:
    typedef std::function<void(ByteBuffer)> EventCallback;
    typedef std::shared_ptr<Kcp> SP;
    Kcp();
    ~Kcp();

    static uint32_t clock();
    bool create(uint32_t conv, sockaddr_in addr, int sock = -1);
    bool installReadEvent(EventCallback callback);
    bool setCloseData(const uint8_t *data, size_t len);

    std::string dumpPeerAddr() const;
    std::string dumpLocalAddr() const;

private:
    static int output(const char* buf, int len, ikcpcb* kcp, void* user);
    int send(const void *buf, size_t len);
    int recv(void *buf, size_t len);
    int recv(ByteBuffer &buffer);
    void update();
    int input(const void *buf, size_t len);
    void callback(ByteBuffer buf);
    bool valid();
    void release();

private:
    int             mSocket;
    ikcpcb*         mKcpHandle;
    uint8_t *       mClosingData;
    size_t          mDataSize;

    EventCallback   mCallback;
    sockaddr_in     mPeerAddr;
    sockaddr_in     mLcoalAddr;
};

} // namespace eular

#endif // __EULAR_KCP_H__

/**
 * kcp的send只将数据放到发送队列
 * 
 * kcpmanager里为1(main) + n线程，由一个线程来接收事件，其他线程具体处理套接字的读事件
 * 
 * kcpmanager的main线程来主发送和接收，
 */