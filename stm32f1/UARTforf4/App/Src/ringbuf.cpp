/* ringbuf.cpp - C++ RingBuffer class implementation */
#include "ringbuf.h"

// 构造函数
RingBuffer::RingBuffer()
{
    init();
}

// 初始化
void RingBuffer::init(void)
{
    head = 0;
    tail = 0;
}

// 推入数据(中断里调用)
int RingBuffer::push(uint8_t c)
{
    uint16_t next = (head + 1) & MASK;
    if (next == tail)
        return -1; // 满了,丢
    buf[head] = c;
    head = next;
    return 0;
}

// 弹出数据(主循环调用)
int RingBuffer::pop(void)
{
    if (tail == head)
        return -1; // 空
    uint8_t c = buf[tail];
    tail = (tail + 1) & MASK;
    return c;
}

// 判断是否为空
bool RingBuffer::isEmpty(void) const
{
    return tail == head;
}

// 判断是否已满
bool RingBuffer::isFull(void) const
{
    return ((head + 1) & MASK) == tail;
}

// 清空缓冲区
void RingBuffer::clear(void)
{
    head = tail = 0;
}
