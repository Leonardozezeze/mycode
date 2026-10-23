/* ringbuf.h - C++ RingBuffer class */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

#ifdef __cplusplus

// 环形缓冲区大小(必须是 2 的幂)
#ifndef RINGBUF_SIZE
#define RINGBUF_SIZE 256
#endif

class RingBuffer {
private:
    static const uint16_t SIZE = RINGBUF_SIZE;
    static const uint16_t MASK = SIZE - 1;

    volatile uint16_t head; // 写入位置(中断里改)
    volatile uint16_t tail; // 读取位置(主循环改)
    uint8_t buf[SIZE];      // 固定大小的缓冲区

public:
    // 构造函数
    RingBuffer();

    // 初始化(构造后自动初始化,此方法保留用于兼容)
    void init(void);

    // 中断里调用,满返回 -1
    int push(uint8_t c);

    // 主循环调用,空返回 -1
    int pop(void);

    // 判断是否为空
    bool isEmpty(void) const;

    // 判断是否已满
    bool isFull(void) const;

    // 清空缓冲区
    void clear(void);
};

#endif // __cplusplus

#endif // RINGBUF_H
