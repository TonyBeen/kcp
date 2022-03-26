/*************************************************************************
    > File Name: kcp.h
    > Author: hsz
    > Brief:
    > Created Time: Sun 13 Mar 2022 02:01:51 PM CST
 ************************************************************************/

#ifndef __EULAR_KCP_H__
#define __EULAR_KCP_H__

#include "ikcp.h"
#include "address.h"
#include <utils/utils.h>
#include <utils/timer.h>
#include <functional>
#include <memory>

namespace eular {

class Kcp {
    DISALLOW_COPY_AND_ASSIGN(Kcp);
public:
    Kcp();
    virtual ~Kcp();

    virtual ssize_t send(const char *buf, uint32_t len);
    virtual ssize_t recv(char *buf, uint32_t len);

protected:
    /**
     * @brief 套接字发送函数，子类需实现
     * 
     * @param buf 
     * @param len 
     * @return ssize_t 返回发送的字节数
     */
    virtual ssize_t output(const char *buf, uint32_t len) = 0;
    /**
     * @brief 将接收到的数据输入到kcp中
     * 
     * @param buf 缓存
     * @param len 长度
     * @return ssize_t 
     */
    virtual ssize_t input(const char *buf, uint32_t len) = 0;
    static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user);
    virtual bool create();
    void onTimeOut();

protected:
    uint32_t        mFlagConv;      // 交流标志，唯一
    ikcpcb*         mKcpHandle;     // kcp句柄
    TimerManager    mTimerMan;      // 定时器
};

} // namespace eular

#endif // __EULAR_KCP_H__