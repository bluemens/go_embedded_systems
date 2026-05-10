#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <libusb-1.0/libusb.h>
#include <stdint.h>

// 控制器设备结构（仅保留一个）
struct controller_list {
    struct libusb_device_handle *device1;
    uint8_t device1_addr;
};

// 接收包结构（固定7字节）
struct controller_pkt {
    uint8_t const1;
    uint8_t const2;
    uint8_t const3;
    uint8_t dir_x;
    uint8_t dir_y;
    uint8_t ab;
    uint8_t rl;
};

// 监听线程参数结构
struct args_list {
    struct controller_list devices;
    char *buttons;  // 至少为 6 字节的字符数组
    int mode;       // 如果为 1，强制所有按钮为“按下”
    int print;      // 如果为 1，每次打印buttons状态
};

// 打开一个控制器，初始化libusb
struct controller_list open_controller();

// 用于监听controller输入（适用于pthread）
void *listen_controller(void *arg);

// 将controller_pkt中的方向和A键映射到"LRUDA"格式
void detect_presses(struct controller_pkt pkt, char *buttons, int mode);

#endif