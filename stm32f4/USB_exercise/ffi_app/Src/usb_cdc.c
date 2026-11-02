#include "usb_cdc.h"
#include "usbd_cdc_if.h"

#ifndef USB_CDC_TX_RETRY
#define USB_CDC_TX_RETRY 100000u
#endif

/*
 * CDC_Transmit_FS 只保存调用者缓冲区的指针，真正的 USB 中断完成时
 * 才去读它。所以这里必须用静态缓冲区，不能把调用者的临时缓冲
 * （尤其是 Rust 栈上的 &str）直接传进去。
 */
static uint8_t cdc_tx_buf[APP_TX_DATA_SIZE];

extern USBD_HandleTypeDef hUsbDeviceFS;

/* 上一次发送是否已经完成（TxState 由 USB 中断清零） */
static int cdc_tx_ready(void)
{
    if (hUsbDeviceFS.pClassData == 0) {
        return 0;
    }
    volatile USBD_CDC_HandleTypeDef *hcdc =
        (volatile USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    return hcdc->TxState == 0;
}

int usb_cdc_write(const uint8_t *data, uint16_t len)
{
    uint32_t n;

    if (data == 0 || len == 0) {
        return 0;
    }
    if (len > sizeof(cdc_tx_buf)) {
        return -1;
    }

    /* 等上一次传输完成，否则会覆盖还在发送中的旧数据 */
    for (n = 0; n < USB_CDC_TX_RETRY; n++) {
        if (cdc_tx_ready()) {
            break;
        }
    }
    if (n == USB_CDC_TX_RETRY) {
        return -2;
    }

    for (uint16_t i = 0; i < len; i++) {
        cdc_tx_buf[i] = data[i];
    }

    for (n = 0; n < USB_CDC_TX_RETRY; n++) {
        uint8_t status = CDC_Transmit_FS(cdc_tx_buf, len);
        if (status == USBD_OK) {
            return 0;
        }
        if (status == USBD_FAIL) {
            return -3; /* 还没有枚举成功 */
        }
        /* USBD_BUSY：中断刚好又占用了发送，继续等 */
    }
    return -2;
}
