/*************************************************************************
    > File Name: kcpmanager.h
    > Author: hsz
    > Brief:
    > Created Time: Tue 07 Jun 2022 02:00:21 PM CST
 ************************************************************************/

#ifndef __EULAR_KCP_MANAGER_H__
#define __EULAR_KCP_MANAGER_H__

#include "kcp.h"
#include <utils/mutex.h>
#include <sys/epoll.h>
#include <thread>
#include <vector>
#include <map>
#include <list>
#include <mutex>
#include <atomic>
#include <iostream>

namespace eular {

class KcpManager
{
public:
    KcpManager();
    ~KcpManager();

    bool start(size_t threads = 3);
    void stop();

    bool addKcp(Kcp::SP kcp);
    void delKcp(Kcp::SP kcp);
    ssize_t send(Kcp::SP kcp, const void *buf, size_t len);
    ssize_t send(Kcp::SP kcp, const ByteBuffer &buf);

private:
    void eventloop();
    void processloop();
    void onEventLoopExit();

private:
    int mEpollFd;
    uint16_t  mInterValue;  // 内部时钟间隔

    std::atomic<uint16_t>   mKcpCount;
    std::map<int, Kcp::SP>  mKcpMap;
    RWMutex mKcpMapMutex;
    std::list<std::pair<Kcp::SP, ByteBuffer>> mSendQueue;
    Mutex mSendQueueMutex;

    std::list<std::function<void()>> mCallbackQueue; // 事件回调队列
    Mutex mCallbackMutex;
    std::atomic<bool>               mKeepLoop;      // 是否保持循环
    std::shared_ptr<std::thread>    mMainThread;    // 事件线程
    std::vector<std::shared_ptr<std::thread>> mThreadPool;    // 事务处理线程
};

} // namespace eular

#endif // __EULAR_KCP_MANAGER_H__
