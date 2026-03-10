#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <lvgl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "display_thread.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(Display_TASK, LOG_LEVEL_INF);

/* --- 硬件常量定义 --- */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
// STM32F429I-DISC1 默认显存起始地址 (需确保与设备树配置一致)
#define SDRAM_FB_ADDR 0xD0000000 

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static lv_chart_series_t * ser1 = NULL;
static lv_obj_t * main_chart = NULL;
static lv_obj_t * main_bar = NULL;

/**
 * @brief 按钮事件：处理清空逻辑
 */
static void btn_reset_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_PRESSED) {
        LOG_INF("UI: Reset Triggered");
        if (main_chart && ser1) {
            lv_chart_set_all_value(main_chart, ser1, 0);
            lv_chart_refresh(main_chart);
        }
    }
}

/**
 * @brief 数据更新定时器
 */
static void ui_timer_cb(lv_timer_t * timer) {
    static int val = 0;
    val = (val + 1) % 101;
    lv_bar_set_value(main_bar, val, LV_ANIM_ON);

    uint32_t rnd = (k_cycle_get_32() % 60) + 20;
    lv_chart_set_next_value(main_chart, ser1, rnd);
}

/**
 * @brief 构建最终演示界面
 */
void create_demo_ui(void) {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_palette_darken(LV_PALETTE_GREY, 4), 0);

    /* 标题 */
    lv_obj_t * title = lv_label_create(screen);
    lv_label_set_text(title, "SYSTEM MONITOR v1.0");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_LIME), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* 进度条 */
    main_bar = lv_bar_create(screen);
    lv_obj_set_size(main_bar, 200, 12);
    lv_obj_align(main_bar, LV_ALIGN_TOP_MID, 0, 40);

    /* 折线图 */
    main_chart = lv_chart_create(screen);
    lv_obj_set_size(main_chart, 220, 140);
    lv_obj_align(main_chart, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(main_chart, LV_CHART_TYPE_LINE);
    lv_obj_set_style_bg_color(main_chart, lv_color_black(), 0);
    ser1 = lv_chart_add_series(main_chart, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);

    /* RESET 按钮 */
    lv_obj_t * btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 120, 45);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置按下反馈
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), LV_STATE_PRESSED);

    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "RESET DATA");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, btn_reset_cb, LV_EVENT_ALL, NULL);

    lv_timer_create(ui_timer_cb, 100, NULL);
}

/**
 * @brief 显示线程入口
 */
void display_thread_entry(void) 
{
    // 稍微多等一会儿，确保外设上电稳定
    k_msleep(800); 

    LOG_INF("Display Thread Init...");

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not found!");
        return;
    }

    /* --- 步骤 A: 物理显存清零 --- */
    // 用深灰色彻底覆盖 SDRAM 中的随机数据
    uint16_t *fb = (uint16_t *)SDRAM_FB_ADDR;
    for(int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        fb[i] = 0x2104; // 深灰色 RGB565
    }
    LOG_INF("SDRAM Framebuffer cleaned.");

    // 关闭消隐，开启背光显示
    display_blanking_off(display_dev);

    /* --- 步骤 B: 创建 UI --- */
    create_demo_ui();

    /* --- 步骤 C: 任务主循环 --- */
    while (1) {
        // lv_task_handler 会在此处处理 DMA2D 搬运及输入队列读取
        uint32_t next_run = lv_task_handler();
        
        // 动态睡眠：保持 5ms-30ms 之间，给 Input 系统留出处理带宽
        if (next_run < 5) next_run = 5;
        if (next_run > 30) next_run = 30;
        
        k_msleep(next_run);
    }
}

/* 线程定义 (降低优先级以确保 Input 子系统响应) */
#define DISPLAY_STACK_SIZE 8192
#define DISPLAY_PRIORITY 12 // 优先级调低一点

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE, 
                display_thread_entry, NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, 0);