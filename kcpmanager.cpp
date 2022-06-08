/*************************************************************************
    > File Name: kcpmanager.cpp
    > Author: hsz
    > Brief:
    > Created Time: Tue 07 Jun 2022 02:00:25 PM CST
 ************************************************************************/

#include "kcpmanager.h"
#include <log/log.h>

#define LOG_TAG "kcpmanager"

namespace eular {

Sem gSem(0);
static const uint16_t gEventSize = 4096;

KcpManager::KcpManager() :
    mKeepLoop(false),
    mEpollFd(-1),
    mMainThread(nullptr),
    mInterValue(20),
    mKcpCount(0)
{

}

KcpManager::~KcpManager()
{
    stop();
}

bool KcpManager::start(size_t threads)
{
    LOG_ASSERT2(threads > 0);
    if (mKeepLoop) {
        return true;
    }

    mKeepLoop = true;
    mMainThread.reset(new (std::nothrow)std::thread(&KcpManager::eventloop, this));
    if (mMainThread == nullptr) {
        LOGW("new failed");
        return false;
    }

    mMainThread->detach();

    if (gSem.timedwait(500) == false) {
        mKeepLoop = false;
        mMainThread.reset();
        LOGW("timeout");
        return false;
    }

    std::shared_ptr<std::thread> ptr;
    mThreadPool.resize(threads);
    for (size_t i = 0; i < threads; ++i) {
        ptr.reset(new (std::nothrow)std::thread(&KcpManager::processloop, this));
        LOG_ASSERT2(ptr != nullptr);
        mThreadPool.push_back(ptr);
    }

    return true;
}

void KcpManager::stop()
{
    if (!mKeepLoop) {
        return;
    }

    mKeepLoop = false;
}

bool KcpManager::addKcp(Kcp::SP kcp)
{
    if (kcp == nullptr) {
        return false;
    }

    if (mKcpCount > gEventSize) {
        LOGW("epoll event are full");
        return false;
    }

    epoll_event ev;
    ev.data.ptr = kcp.get();
    ev.events = EPOLLET | EPOLLIN;

    int code = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, kcp->mSocket, &ev);
    if (code < 0) {
        return false;
    }

    WRAutoLock<RWMutex> lock(mKcpMapMutex);
    auto ret = mKcpMap.insert(std::make_pair(kcp->mSocket, kcp));
    if (ret.second == true) {
        mKcpCount++;
    }
    return ret.second;
}

void KcpManager::delKcp(Kcp::SP kcp)
{
    if (kcp == nullptr) {
        return;
    }

    epoll_ctl(mEpollFd, EPOLL_CTL_DEL, kcp->mSocket, nullptr);
    WRAutoLock<RWMutex> lock(mKcpMapMutex);
    mKcpMap.erase(kcp->mSocket);
    mKcpCount--;
}

ssize_t KcpManager::send(Kcp::SP kcp, const void *buf, size_t len)
{
    return send(kcp, ByteBuffer((const char *)buf, len));
}

ssize_t KcpManager::send(Kcp::SP kcp, const ByteBuffer &buf)
{
    if (kcp == nullptr) {
        return -1;
    }

    AutoLock<Mutex> lock(mSendQueueMutex);
    mSendQueue.push_back(std::make_pair(kcp, buf));
    return 0;
}

void KcpManager::eventloop()
{
    LOGD("%s()", __func__);
    mEpollFd = epoll_create(gEventSize);
    if (mEpollFd < 0) {
        perror("epoll_create error");
        return;
    }

    uint8_t recvbuf[1024] = {0};
    epoll_event events[gEventSize];
    gSem.post();

    while (mKeepLoop) {
        int nev = epoll_wait(mEpollFd, events, gEventSize, mInterValue);
        if (nev < 0) {
            LOGE("epoll_wait error. [%d,%s]", errno, strerror(errno));
            break;
        }

        {
            AutoLock<Mutex> lock(mSendQueueMutex);
            for (auto &it : mSendQueue) {
                std::cout << it.first->send(it.second.data(), it.second.size()) << std::endl;
            }
            mSendQueue.clear();
        }

        for (auto &it : mKcpMap) {
            it.second->update();
        }

        for (int i = 0; i < nev; ++i) {
            void *ptr = events[i].data.ptr;
            uint32_t ev = events[i].events;
            LOG_ASSERT2(ptr != nullptr);
            Kcp* kcp = static_cast<Kcp *>(ptr);

            LOGD("event 0x%x", ev);
            if (ev & (EPOLLHUP | EPOLLERR)) {
                delKcp(kcp->shared_from_this());
                LOGE("event error. [%d,%s]", errno, strerror(errno));
                continue;
            }
            if (ev & EPOLLIN) {
                ByteBuffer buffer;
                while (true) {
                    int nrecv = ::recvfrom(kcp->mSocket, recvbuf, sizeof(recvbuf), 0, nullptr, nullptr);
                    if (nrecv <= 0) {
                        if (errno != EAGAIN) {
                            LOGE("recvfrom error. [%d,%s]", errno, strerror(errno));
                        }
                        break;
                    }
                    buffer.append(recvbuf, nrecv);
                }

                if (buffer.size() > 0) {
                    kcp->input(buffer.data(), buffer.size());
                    if (kcp->recv(buffer) > 0) {
                        AutoLock<Mutex> lock(mCallbackMutex);
                        mCallbackQueue.push_back(std::bind(&Kcp::callback, kcp, buffer));
                    }
                }
            }
        }
    }

    onEventLoopExit();
}

void KcpManager::processloop()
{
    std::function<void()> callback;
    while (mKeepLoop) {
        {
            AutoLock<Mutex> lock(mCallbackMutex);
            if (mCallbackQueue.size() == 0) {
                continue;
            }
            callback = mCallbackQueue.front();
            mCallbackQueue.pop_front();
        }
        callback();
    }
}

void KcpManager::onEventLoopExit()
{
    mMainThread.reset();
    mKeepLoop = false;
    close(mEpollFd);
    mEpollFd = -1;
    mThreadPool.clear();
}

} // namespace eular
