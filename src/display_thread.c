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

/* --- 配置常量 --- */
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
// STM32F429I-DISC1 默认显存起始地址 (Bank 2)
#define SDRAM_FB_ADDR 0xD0000000 

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* --- 全局指针 --- */
static lv_style_t style_bar_bg;
static lv_style_t style_bar_indic;
static lv_chart_series_t * ser1 = NULL;
static lv_obj_t * main_chart = NULL;
static lv_obj_t * main_bar = NULL;

/**
 * @brief 按钮点击回调函数
 */
static void btn_reset_cb(lv_event_t * e) 
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * chart = (lv_obj_t *)lv_event_get_user_data(e);

    // 只要手指一碰触到按钮，就会进入这里
    if(code == LV_EVENT_PRESSED) {
        LOG_INF("Button PRESSED! (Signal received)");
        // 可以在这里立即执行重置逻辑，跳过点击判定
        if (chart && ser1) {
            lv_chart_set_all_value(chart, ser1, 0);
            lv_chart_refresh(chart);
        }
    }
    
    // 原有的点击逻辑（需要按下并抬起，且期间没有大幅位移）
    if(code == LV_EVENT_CLICKED) {
        LOG_INF("Button CLICKED! (Gesture recognized)");
    }
}

/**
 * @brief 定时器回调：模拟数据更新
 */
static void ui_update_timer_cb(lv_timer_t * timer) 
{
    if (!main_bar || !main_chart || !ser1) return;

    // 更新进度条
    static int bar_val = 0;
    bar_val = (bar_val + 2) % 102;
    lv_bar_set_value(main_bar, bar_val > 100 ? 100 : bar_val, LV_ANIM_ON);

    // 更新波形图 (随机数模拟信号)
    uint32_t rnd = (k_cycle_get_32() % 80) + 10;
    lv_chart_set_next_value(main_chart, ser1, rnd);
}

/**
 * @brief 现代感 UI 构建 (含启动清场逻辑)
 */
void create_modern_demo_ui(void)
{
    /* --- 1. 强制清场逻辑：确保背景不再花屏 --- */
    lv_obj_t * screen = lv_scr_act();
    // 设置活动屏幕背景为深灰色，防止初始乱码露头
    lv_obj_set_style_bg_color(screen, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* --- 2. 创建全屏背景容器作为所有控件的父对象 --- */
    lv_obj_t * bg_base = lv_obj_create(screen);
    lv_obj_set_size(bg_base, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg_base, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_radius(bg_base, 0, 0);
    lv_obj_set_style_border_width(bg_base, 0, 0);
    lv_obj_align(bg_base, LV_ALIGN_CENTER, 0, 0);

    /* --- 3. 样式初始化 --- */
    lv_style_init(&style_bar_bg);
    lv_style_set_border_width(&style_bar_bg, 2);
    lv_style_set_border_color(&style_bar_bg, lv_palette_main(LV_PALETTE_BLUE_GREY));
    lv_style_set_radius(&style_bar_bg, LV_RADIUS_CIRCLE);

    lv_style_init(&style_bar_indic);
    lv_style_set_bg_opa(&style_bar_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bar_indic, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_bar_indic, lv_palette_main(LV_PALETTE_CYAN));
    lv_style_set_bg_grad_dir(&style_bar_indic, LV_GRAD_DIR_HOR);
    lv_style_set_radius(&style_bar_indic, LV_RADIUS_CIRCLE);

    /* --- 4. 控件创建 --- */
    lv_obj_t * title = lv_label_create(bg_base);
    lv_label_set_text(title, "System Monitor (DMA2D)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    main_bar = lv_bar_create(bg_base);
    lv_obj_add_style(main_bar, &style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(main_bar, &style_bar_indic, LV_PART_INDICATOR);
    lv_obj_set_size(main_bar, 200, 20);
    lv_obj_align(main_bar, LV_ALIGN_TOP_MID, 0, 45);
    lv_bar_set_range(main_bar, 0, 100);

    main_chart = lv_chart_create(bg_base);
    lv_obj_set_size(main_chart, 220, 150);
    lv_obj_align(main_chart, LV_ALIGN_CENTER, 0, 40);
    lv_chart_set_range(main_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(main_chart, 5, 5); 
    lv_chart_set_type(main_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(main_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_color(main_chart, lv_palette_darken(LV_PALETTE_GREY, 4), 0);

    ser1 = lv_chart_add_series(main_chart, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);

    /* RESET 按钮 */
    lv_obj_t * btn = lv_btn_create(bg_base);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* 重点 1：禁用父对象的滚动，防止点击被滑动拦截 */
    lv_obj_clear_flag(bg_base, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 重点 2：将事件改为 ALL，或者同时监听 PRESSED 和 CLICKED */
    // 我们这里改为监听 ALL，然后在回调里用 switch/if 判断
    lv_obj_add_event_cb(btn, btn_reset_cb, LV_EVENT_ALL, main_chart);
    
    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "RESET");
    lv_obj_center(btn_label);

    /* 绑定事件，注意传递 main_chart 指针 */
    lv_obj_add_event_cb(btn, btn_reset_cb, LV_EVENT_CLICKED, main_chart);

    /* 启动定时器：50ms 刷新一次逻辑 */
    lv_timer_create(ui_update_timer_cb, 50, NULL);

    /* --- 5. 关键：强制使全屏失效，触发逻辑上的第一次全屏绘制 --- */
    lv_obj_invalidate(screen);

    lv_obj_move_foreground(btn);
}

void display_thread_entry(void) 
{
    LOG_INF("Display Thread starting...");
    k_msleep(500); // 等待硬件稳定

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready!");
        return;
    }

    /* --- 硬件级“暴力清屏” --- */
    // 直接操作 SDRAM 显存地址，将所有像素设为黑色/灰色
    // 0x3186 是 RGB565 的深灰色
    uint16_t *fb = (uint16_t *)SDRAM_FB_ADDR;
    for(int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        fb[i] = 0x3186; 
    }
    LOG_INF("Hardware Framebuffer cleared.");

    // 打开显示背光/消隐
    display_blanking_off(display_dev);

    /* 创建 UI */
    create_modern_demo_ui();

    while (1) {
        /* lv_task_handler 会处理所有 UI 逻辑，包括 DMA2D 搬运局部脏矩形 */
        uint32_t sleep_time = lv_task_handler();
        
        // 动态睡眠：限制在 5ms 到 30ms 之间，平衡响应速度和总线负载
        if (sleep_time < 5) sleep_time = 5;
        if (sleep_time > 30) sleep_time = 30;
        
        k_msleep(sleep_time);
    }
}

/* 定义线程 */
#define DISPLAY_STACK_SIZE 8192
#define DISPLAY_PRIORITY 14 // 优先级略高，确保显示流畅

K_THREAD_DEFINE(display_thread_tid, DISPLAY_STACK_SIZE, 
                display_thread_entry, NULL, NULL, NULL,
                DISPLAY_PRIORITY, 0, 0);