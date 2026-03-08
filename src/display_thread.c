#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include "display_thread.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(Display_TASK, LOG_LEVEL_INF);

// 获取设备树中定义的显示设备 (对应 chosen 节点中的 zephyr,display)
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

void display_thread_entry(void) 
{
    LOG_INF("Display Thread started");

    // 1. 检查显示设备是否准备就绪    
	if (!device_is_ready(display_dev)) {
		LOG_ERR("显示设备未准备就绪！");
		return;
	}

	LOG_INF("显示设备已启动，正在初始化画布...");

	// 2. 创建一个基础对象（类似一个容器或画布）
	lv_obj_t *rect = lv_obj_create(lv_scr_act());
	
	// 3. 设置这个矩形的大小（显示一张 150x150 的图像区域）
	lv_obj_set_size(rect, 150, 150);
	
	// 4. 将它放在屏幕中央
	lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);

	// 5. 修改这个矩形的样式，使其看起来像一个彩色的图像块
	static lv_style_t style;
	lv_style_init(&style);
	// 设置背景颜色为天蓝色
	lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
	// 设置边框
	lv_style_set_border_width(&style, 5);
	lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_RED));
	// 设置圆角
	lv_style_set_radius(&style, 10);
	
	lv_obj_add_style(rect, &style, 0);

	// 6. 添加一个标签显示文字
	lv_obj_t *label = lv_label_create(rect);
	lv_label_set_text(label, "STM32 F429\nDisplay OK!");
	lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

	LOG_INF("开始进入刷新循环...");

    while (1) {
        lv_task_handler(); 
        k_msleep(30);      
    }
}

#define DISPLAY_STACK_SIZE 8192
#define DISPLAY_PRIORITY 10

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE, 
                display_thread_entry, NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, 0);