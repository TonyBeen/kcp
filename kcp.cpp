/*************************************************************************
    > File Name: kcp.cpp
    > Author: hsz
    > Brief:
    > Created Time: Sun 13 Mar 2022 02:01:55 PM CST
 ************************************************************************/

#include "kcp.h"
#include <utils/exception.h>
#include <log/log.h>

#define LOG_TAG "kcp"

namespace eular {

Kcp::Kcp() :
    mFlagConv(0),
    mKcpHandle(nullptr)
{
    mTimerMan.addTimer(10, std::bind(&Kcp::onTimeOut, this), 10);
    mTimerMan.StartTimer();
    create();
}

Kcp::~Kcp()
{
    if (mKcpHandle) {
        ikcp_release(mKcpHandle);
        mKcpHandle = nullptr;
    }
}

ssize_t Kcp::send(const char *buf, uint32_t len)
{
    assert(mKcpHandle != nullptr);
    return ikcp_send(mKcpHandle, buf, len);
}

ssize_t Kcp::recv(char *buf, uint32_t len)
{
    assert(mKcpHandle != nullptr);
    return -1;
}

int Kcp::kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    Kcp *_kcp = static_cast<Kcp *>(user);
    LOG_ASSERT(_kcp != nullptr, "");
    return _kcp->output(buf, len);
}

bool Kcp::create()
{
    mKcpHandle = ikcp_create(mFlagConv, this);
    if (mKcpHandle == nullptr) {
        throw(eular::Exception("ikcp_create error."));
    }

    mKcpHandle->output = kcp_output;
}

void Kcp::onTimeOut()
{
    if (mKcpHandle) {
        ikcp_update(mKcpHandle, Timer::getCurrentTime(CLOCK_REALTIME));
    }
}

} // namespace eular
