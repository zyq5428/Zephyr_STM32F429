#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <stdio.h>
#include <stdint.h>

#include "display_thread.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(Display_TASK, LOG_LEVEL_INF);

// 获取设备树中定义的显示设备 (对应 chosen 节点中的 zephyr,display)
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* 创建 UI 的主功能函数 */
void create_test_ui(void)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr); // 清除之前所有的对象

    /* --- 1. 背景色块测试 (检查颜色位深和 RGB 顺序) --- */
    // 红色块
    lv_obj_t * red_rect = lv_obj_create(scr);
    lv_obj_set_size(red_rect, 70, 70);
    lv_obj_align(red_rect, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(red_rect, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(red_rect, 0, 0);

    // 绿色块
    lv_obj_t * green_rect = lv_obj_create(scr);
    lv_obj_set_size(green_rect, 70, 70);
    lv_obj_align(green_rect, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(green_rect, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(green_rect, 0, 0);

    // 蓝色块
    lv_obj_t * blue_rect = lv_obj_create(scr);
    lv_obj_set_size(blue_rect, 70, 70);
    lv_obj_align(blue_rect, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(blue_rect, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(blue_rect, 0, 0);

    /* --- 2. 渐变色测试 (检查 SDRAM 搬运大面积数据的稳定性) --- */
    lv_obj_t * grad_rect = lv_obj_create(scr);
    lv_obj_set_size(grad_rect, 220, 80);
    lv_obj_align(grad_rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_grad_color(grad_rect, lv_palette_main(LV_PALETTE_PURPLE), 0);
    lv_obj_set_style_bg_main_stop(grad_rect, 0, 0);
    lv_obj_set_style_bg_grad_stop(grad_rect, 255, 0);
    lv_obj_set_style_bg_grad_dir(grad_rect, LV_GRAD_DIR_HOR, 0);

    /* --- 3. 极细线条测试 (检查 LTDC 时序是否导致画面模糊或偏移) --- */
    static lv_point_precise_t line_points[] = { {0, 0}, {240, 0} };
    lv_obj_t * line = lv_line_create(scr);
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 50);

    /* --- 4. 文本清晰度与抗锯齿对比 --- */
    lv_obj_t * label_small = lv_label_create(scr);
    lv_label_set_text(label_small, "Small Text (18px) - Check for Blur");
    lv_obj_set_style_text_font(label_small, &lv_font_montserrat_14, 0);
    lv_obj_align(label_small, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t * label_large = lv_label_create(scr);
    lv_label_set_text(label_large, "Large Text (24px)");
    lv_obj_set_style_text_font(label_large, &lv_font_montserrat_24, 0);
    lv_obj_align(label_large, LV_ALIGN_BOTTOM_MID, 0, -5);
}

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

	/* 创建 UI (此时 LVGL 已经准备好缓冲到 SDRAM) */
    create_test_ui();

    while (1) {
        /* 4. 周期性调用 LVGL 任务处理器 */
        /* LVGL 会在这里根据你设置的双缓冲配置，自动通过 LTDC 刷新屏幕 */
        lv_task_handler();
        k_sleep(K_MSEC(5)); // 给系统留一点喘息时间
    }
}

#define DISPLAY_STACK_SIZE 8192
#define DISPLAY_PRIORITY 12

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE, 
                display_thread_entry, NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, 0);