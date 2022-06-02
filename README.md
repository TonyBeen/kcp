# kcp
kcp `https://github.com/skywind3000/kcp`

#### 线程不安全参考
`https://github.com/skywind3000/kcp/issues/123`


kcp只完成算法上的管理，不处理业务逻辑

本此例子就是以单线程来完成收发

#### `TODO`
> 1、处理当探测帧三次无应答则主动通知用户无响应，由用户确认是否断开连接
> 2、