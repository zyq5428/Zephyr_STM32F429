#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "led_thread.h"

// 启用日志记录
LOG_MODULE_REGISTER(LED_TASK, LOG_LEVEL_INF);

/* GPIO_DT_SPEC_GET(node_id, property)： 这是Zephyr提供的另一个宏，
* 用于从指定的设备节点ID (LED1_NODE) 中提取名为 property 的属性值，
* 并将其封装成 struct gpio_dt_spec。
* 这里的 gpios 是一个标准的设备树属性名。
*/
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

void led_thread_entry(void *p1, void *p2, void *p3)
{
    // ... 在这里初始化您的 LED 设备 ...
    LOG_INF("LED Thread started");

    int ret;

	/* 初始化 GPIO (LED) */
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("GPIO device not ready");
        return;
    }

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Error: Failed to configure LED pin (ret=%d)", ret);
        while(1) { return; } 
	}

    /* 主动翻转一次LED，间隔1s */
	k_msleep(1000); // 等待 1 秒
	gpio_pin_toggle_dt(&led);
	LOG_DBG("GPIO LED Toggled Test");


    while (1) {
		/* --- 每 1 秒切换一次 GPIO LED 状态 --- */
        gpio_pin_toggle_dt(&led);
        LOG_DBG("Green LED Toggled");

        /* 休眠 1000ms */
        k_msleep(1000);
    }
}

// 线程栈和定义
#define LED_STACK_SIZE 1024
#define LED_PRIORITY 15

K_THREAD_DEFINE(led_tid, LED_STACK_SIZE, 
                led_thread_entry, NULL, NULL, NULL,
                LED_PRIORITY, 0, 0);
