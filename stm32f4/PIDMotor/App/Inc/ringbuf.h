/* ringbuf.h */
#ifndef RINGBUF_H
#define RINGBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RX_BUF_SIZE 256 /* 必须是 2 的幂，方便用 & 取模 */

typedef struct
{
    volatile uint16_t head; /* 写入位置（中断里改） */
    volatile uint16_t tail; /* 读取位置（主循环改） */
    uint8_t buf[RX_BUF_SIZE];
} ringbuf_t;

extern ringbuf_t g_rx;

void ringbuf_init(ringbuf_t *r);
int ringbuf_push(ringbuf_t *r, uint8_t c); /* 中断里调，满返回 -1 */
int ringbuf_pop(ringbuf_t *r);             /* 主循环调，空返回 -1 */

#ifdef __cplusplus
}
#endif

#endif