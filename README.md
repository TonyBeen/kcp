# kcp
kcp `https://github.com/skywind3000/kcp`

#### 线程不安全参考
`https://github.com/skywind3000/kcp/issues/123`


kcp只完成算法上的管理，不处理业务逻辑

本次例子就是以单线程来完成收发

udp_client.cc和udp_server.cc是一个简单的demo

kcpmanager和kcp可以实现一些复杂的udp收发，kcpmanager内部维护一个简单线程池，和一个主线程
主线程接收事件，处理udp发送，udp接收，并压入回调队列，线程池处理回调，将数据给到具体某一个kcp对象
这部分也算是一个稍微复杂点的demo，因为主线程及线程池做的简单

计划将这部分接入p2p，用作客户端数据传输