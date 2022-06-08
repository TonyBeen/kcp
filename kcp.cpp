/*************************************************************************
    > File Name: kcp.cpp
    > Author: hsz
    > Brief:
    > Created Time: Tue 07 Jun 2022 10:09:07 AM CST
 ************************************************************************/

#include "kcp.h"
#include <chrono>
#include <fcntl.h>

namespace eular {

Kcp::Kcp()
{
    mSocket = -1;
    mKcpHandle = nullptr;
    mClosingData = nullptr;
    mDataSize = 0;
    mCallback = nullptr;
    memset(&mPeerAddr, 0, sizeof(mPeerAddr));
    memset(&mLcoalAddr, 0, sizeof(mLcoalAddr));
}

Kcp::~Kcp()
{
    release();
}

uint32_t Kcp::clock()
{
    std::chrono::steady_clock::time_point tm = std::chrono::steady_clock::now();
    std::chrono::milliseconds mills = 
        std::chrono::duration_cast<std::chrono::milliseconds>(tm.time_since_epoch());
    return (uint32_t)mills.count();
}

bool Kcp::create(uint32_t conv, sockaddr_in addr, int sock)
{
    if (valid()) {
        return true;
    }

    mKcpHandle = ikcp_create(conv, this);
    if (mKcpHandle == nullptr) {
        return false;
    }
    mKcpHandle->output = Kcp::output;
    ikcp_wndsize(mKcpHandle, 128, 128);
    ikcp_nodelay(mKcpHandle, 0, 40, 2, 1);

    if (sock <= 0) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
            ikcp_release(mKcpHandle);
            mKcpHandle = nullptr;
            return false;
        }
    }
    mSocket = sock;
    mPeerAddr = addr;

    uint32_t flag = fcntl(mSocket, F_GETFL);
    fcntl(mSocket, F_SETFL, flag | O_NONBLOCK);

    return true;
}

bool Kcp::installReadEvent(EventCallback callback)
{
    mCallback = callback;
    return true;
}

/**
 * @brief 设置当关闭套接字前要发送的数据
 * 
 * @param data 
 * @param len 
 * @return true 
 * @return false 
 */
bool Kcp::setCloseData(const uint8_t *data, size_t len)
{
    if (!data || !len) {
        return true;
    }
    mClosingData = (uint8_t *)malloc(len + 1);
    if (mClosingData == nullptr) {
        return false;
    }
    mDataSize = len;
    memcpy(mClosingData, data, len);
    return true;
}

std::string Kcp::dumpPeerAddr() const
{
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%s:%d", inet_ntoa(mPeerAddr.sin_addr), ntohs(mPeerAddr.sin_port));
    return std::string(buf);
}

std::string Kcp::dumpLocalAddr() const
{
    socklen_t len = sizeof(mLcoalAddr);
    int code = getsockname(mSocket, (sockaddr *)&mLcoalAddr, &len);
    if (code) {
        return std::string("");
    }
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%s:%d", inet_ntoa(mLcoalAddr.sin_addr), ntohs(mLcoalAddr.sin_port));
    return std::string(buf);
}

int Kcp::output(const char* buf, int len, ikcpcb* kcp, void* user)
{
    Kcp *k = static_cast<Kcp *>(user);
    return ::sendto(k->mSocket, buf, len, 0, (sockaddr *)&k->mPeerAddr, sizeof(k->mPeerAddr));
}

int Kcp::send(const void *buf, size_t len)
{
    if (mKcpHandle) {
        return ikcp_send(mKcpHandle, (const char *)buf, len);
    }
    return -1;
}

int Kcp::recv(void *buf, size_t len)
{
    if (mKcpHandle) {
        return ikcp_recv(mKcpHandle, (char *)buf, len);
    }

    return -1;
}

int Kcp::recv(ByteBuffer &buffer)
{
    if (mKcpHandle) {
        int peekSize = ikcp_peeksize(mKcpHandle);
        if (peekSize < 0) {
            printf("no data\n");
            return -1;
        }
        char *buf = (char *)malloc(peekSize + 1);
        if (buf == nullptr) {
            printf("no memory\n");
            return -2;
        }
        if (ikcp_recv(mKcpHandle, buf, peekSize) < 0) {
            free(buf);
            printf("ikcp_recv error\n");
            return -3;
        }

        buffer.set((uint8_t *)buf, peekSize);
        free(buf);
        return peekSize;
    }

    return -2;
}

void Kcp::update()
{
    if (mKcpHandle) {
        ikcp_update(mKcpHandle, clock());
    }
}

int Kcp::input(const void *buf, size_t len)
{
    if (mKcpHandle) {
        return ikcp_input(mKcpHandle, (const char *)buf, len);
    }

    return -1;
}

void Kcp::callback(ByteBuffer buf)
{
    if (mCallback) {
        mCallback(buf);
    }
}

bool Kcp::valid()
{
    return mSocket > 0 && mKcpHandle != nullptr;
}

void Kcp::release()
{
    if (mSocket > 0) {
        close(mSocket);
        mSocket = -1;
    }

    if (mClosingData) {
        free(mClosingData);
        mClosingData = nullptr;
    }

    ikcp_release(mKcpHandle);
}

} // namespace eular
