#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
// #include <lvgl.h>
#include <stdio.h>
#include <stdint.h>

#include "display_thread.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(Display_TASK, LOG_LEVEL_INF);

// 获取设备树中定义的显示设备 (对应 chosen 节点中的 zephyr,display)
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* 对应设备树中的 width 和 height */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

void display_thread_entry(void) 
{
	k_msleep(150);

    LOG_INF("Display Thread started");

    // 1. 检查显示设备是否准备就绪    
	if (!device_is_ready(display_dev)) {
		LOG_ERR("显示设备未准备就绪！");
		return;
	}

	display_blanking_off(display_dev);
    // k_msleep(200);

	/* 获取屏幕信息 */
    struct display_capabilities caps;
    display_get_capabilities(display_dev, &caps);

    /* * 在 LTDC 驱动中，如果你设置了 ext-sdram，
     * 显存通常由驱动自动管理。我们可以直接向屏幕写入数据。
     */
    struct display_buffer_descriptor desc;
    desc.buf_size = SCREEN_WIDTH * SCREEN_HEIGHT * 2; // RGB565 每个像素2字节
    desc.width = SCREEN_WIDTH;
    desc.height = SCREEN_HEIGHT;
    desc.pitch = SCREEN_WIDTH; // 每行像素数

    /* 定义一个颜色缓冲区（指向我们验证过的 SDRAM 地址） */
    uint16_t *fb = (uint16_t *)0xD0000000;
	while (1) {
        // --- 实验：刷全屏红色 ---
        LOG_DBG("刷新屏幕: 红色");
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            fb[i] = 0xF800; // RGB565 红色
        }
        // 将缓冲区内容更新到屏幕
        display_write(display_dev, 0, 0, &desc, fb);
        k_msleep(2000);

        // --- 实验：刷全屏蓝色 ---
        LOG_DBG("刷新屏幕: 蓝色");
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            fb[i] = 0x001F; // RGB565 蓝色
        }
        display_write(display_dev, 0, 0, &desc, fb);
        k_msleep(2000);
    }
}

#define DISPLAY_STACK_SIZE 8192
#define DISPLAY_PRIORITY 12

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE, 
                display_thread_entry, NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, 0);