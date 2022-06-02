//=====================================================================
//
// KCP - A Better ARQ Protocol Implementation
// skywind3000 (at) gmail.com, 2010-2011
//  
// Features:
// + Average RTT reduce 30% - 40% vs traditional ARQ like tcp.
// + Maximum RTT reduce three times vs tcp.
// + Lightweight, distributed as a single source file.
//
//=====================================================================
#ifndef __IKCP_H__
#define __IKCP_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#ifndef INLINE
#if defined(__GNUC__)

#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define INLINE         __inline__ __attribute__((always_inline))
#else
#define INLINE         __inline__
#endif

#elif (defined(_MSC_VER) || defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE 
#endif
#endif


//=====================================================================
// QUEUE DEFINITION                                                  
//=====================================================================
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


//---------------------------------------------------------------------
// queue init                                                         
//---------------------------------------------------------------------
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


//---------------------------------------------------------------------
// queue operation                     
//---------------------------------------------------------------------
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )
	

#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)


#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

#endif


//---------------------------------------------------------------------
// WORD ORDER
//---------------------------------------------------------------------
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MISPEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) 
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif



//=====================================================================
// SEGMENT
//=====================================================================
struct IKCPSEG
{
	struct IQUEUEHEAD node;
	uint32_t conv;		// 连接号。UDP是无连接的，conv用于表示来自哪个客户端。
	/**
	 * @brief 命令字段
	 * IKCP_CMD_PUSH 和 IKCP_CMD_ACK 关联；IKCP_CMD_WASK 和 IKCP_CMD_WINS 关联。
	 * IKCP_CMD_PUSH	数据推送命令
	 * IKCP_CMD_ACK 	确认命令
	 * IKCP_CMD_WASK 	接收窗口大小询问命
	 * IKCP_CMD_WINS	接收窗口大小告知命令
	 */
	uint32_t cmd;
	uint32_t frg;		// 分片：用户数据可能会被分成多个KCP包，发送出去
	uint32_t wnd;		// 接收窗口大小，发送方的发送窗口不能超过接收方给出的数值
	uint32_t ts;		// 时间序列
	uint32_t sn;		// 序列号
	uint32_t una;		// 下一个可接收的序列号。其实就是确认号(代表前面的数据包都收到了)，收到sn=10的包，una为11
	uint32_t len;		// 数据长度
	uint32_t resendts;	// 重发时间序列
	uint32_t rto;		// 超时重传时间
	uint32_t fastack;	// 快速重传机制，记录被跳过的次数，超过次数进行快速重传
	uint32_t xmit;		// 发送次数
	char data[1];		// 数据
};


//---------------------------------------------------------------------
// IKCPCB
//---------------------------------------------------------------------
struct IKCPCB
{
	uint32_t conv, mtu, mss, state;
	uint32_t snd_una, snd_nxt, rcv_nxt;
	uint32_t ts_recent, ts_lastack, ssthresh;
	int32_t rx_rttval, rx_srtt, rx_rto, rx_minrto;
	uint32_t snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
	uint32_t current, interval, ts_flush, xmit;
	uint32_t nrcv_buf, nsnd_buf;
	uint32_t nrcv_que, nsnd_que;
	uint32_t nodelay, updated;
	uint32_t ts_probe, probe_wait;
	uint32_t dead_link, incr;
	struct IQUEUEHEAD snd_queue;
	struct IQUEUEHEAD rcv_queue;
	struct IQUEUEHEAD snd_buf;
	struct IQUEUEHEAD rcv_buf;
	uint32_t *acklist;
	uint32_t ackcount;
	uint32_t ackblock;
	void *user;
	char *buffer;
	int fastresend;
	int nocwnd;
	int logmask;
	int (*output)(const char *buf, int len, struct IKCPCB *kcp, void *user);
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};


typedef struct IKCPCB ikcpcb;

#define IKCP_LOG_OUTPUT			1
#define IKCP_LOG_INPUT			2
#define IKCP_LOG_SEND			4
#define IKCP_LOG_RECV			8
#define IKCP_LOG_IN_DATA		16
#define IKCP_LOG_IN_ACK			32
#define IKCP_LOG_IN_PROBE		64
#define IKCP_LOG_IN_WINS		128
#define IKCP_LOG_OUT_DATA		256
#define IKCP_LOG_OUT_ACK		512
#define IKCP_LOG_OUT_PROBE		1024
#define IKCP_LOG_OUT_WINS		2048

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// interface
//---------------------------------------------------------------------

// create a new kcp control object, 'conv' must equal in two endpoint
// from the same connection. 'user' will be passed to the output callback
// output callback can be setup like this: 'kcp->output = my_udp_output'
ikcpcb* ikcp_create(uint32_t conv, void *user);

// release kcp control object
void ikcp_release(ikcpcb *kcp);

int ikcp_recv(ikcpcb *kcp, char *buffer, int len);
int ikcp_send(ikcpcb *kcp, const char *buffer, int len);

// update state (call it repeatedly, every 10ms-100ms)
// 'current' - current timestamp in millisec
void ikcp_update(ikcpcb *kcp, uint32_t current);
uint32_t ikcp_check(const ikcpcb *kcp, uint32_t current);

// when you received a low level packet (eg. UDP packet), call it
int ikcp_input(ikcpcb *kcp, const char *data, long size);
void ikcp_flush(ikcpcb *kcp);

int ikcp_peeksize(const ikcpcb *kcp);

// change MTU size, default is 14000
int ikcp_setmtu(ikcpcb *kcp, int mtu);

// set maximum window size: sndwnd=32, rcvwnd=32 by default
int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);

// get how many packet is waiting to be sent
int ikcp_waitsnd(const ikcpcb *kcp);

/**
 * @brief 极速模式ikcp_nodelay(kcp, 1, 10, 2, 1)
 * 
 * @param kcp kcp句柄
 * @param nodelay 是否启用 nodelay模式，0不启用；1启用。
 * @param interval 协议内部工作的 interval，单位毫秒，比如 10ms或者 20ms
 * @param resend 快速重传模式，默认0关闭，可以设置2（2次ACK跨越将会直接重传）
 * @param nc 是否关闭拥塞控制，默认是0代表不关闭，1代表关闭。
 * @return int 
 */
int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc);

int ikcp_rcvbuf_count(const ikcpcb *kcp);
int ikcp_sndbuf_count(const ikcpcb *kcp);

void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif


