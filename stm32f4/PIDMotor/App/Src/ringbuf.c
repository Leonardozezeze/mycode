#include "ringbuf.h"
ringbuf_t g_rx;
void ringbuf_init(ringbuf_t *r)
{
    r->head=r->tail=0;
}

int ringbuf_push(ringbuf_t *r, uint8_t c)
{
    uint16_t next = (r->head + 1) & (RX_BUF_SIZE - 1);
    if (next == r->tail)
        return -1; /* 满了，丢 */
    r->buf[r->head] = c;
    r->head = next;
    return 0;
}

int ringbuf_pop(ringbuf_t *r)
{
    if (r->tail == r->head)
        return -1; /* 空 */
    uint8_t c = r->buf[r->tail];
    r->tail = (r->tail + 1) & (RX_BUF_SIZE - 1);
    return c;
}