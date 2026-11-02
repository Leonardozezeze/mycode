#ifndef __FFI_USB_CDC_H
#define __FFI_USB_CDC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 供 Rust 调用的 USB CDC 发送接口。
 *
 * 数据会先拷贝进内部静态缓冲区再发送，调用返回后 data 即可释放。
 *
 * 返回值：
 *   0   发送成功（数据已交给 USB 外设，正在发送）
 *  -1  长度超过内部缓冲区（APP_TX_DATA_SIZE）
 *  -2  上一次发送迟迟未完成（USBD_BUSY 超时）
 *  -3  USB 未枚举 / 发送失败（USBD_FAIL）
 */
int usb_cdc_write(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __FFI_USB_CDC_H */
