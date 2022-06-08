/*************************************************************************
    > File Name: main.cc
    > Author: hsz
    > Brief:
    > Created Time: Wed 08 Jun 2022 10:41:59 AM CST
 ************************************************************************/

#include "kcpmanager.h"
#include <iostream>
#include <log/log.h>
#include <log/callstack.h>

#define LOG_TAG "main"

using namespace std;
using namespace eular;

KcpManager km;

void onRecdEvent(ByteBuffer buffer)
{
    printf("%s() recv %s", __func__, buffer.data());
}

void catchSignal(int sig)
{
    CallStack cs;
    cs.update(0);
    switch (sig)
    {
    case SIGSEGV:
        cs.log("SIGSEGV");
        break;
    case SIGABRT:
        cs.log("SIGABRT");
        break;
    default:
        cout << __func__ << "() " << sig << endl;
        break;
    }

    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGSEGV, catchSignal);
    signal(SIGABRT, catchSignal);
    cout << km.start(1) << endl;
    Kcp::SP kcp(new Kcp);
    kcp->installReadEvent(onRecdEvent);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8500);
    cout << kcp->create(0x1024, addr) << endl;
    km.addKcp(kcp);
    km.send(kcp, "Hello", 5);
    while (1) {
    }

    return 0;
}
