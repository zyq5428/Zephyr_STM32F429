/**
 * @file dietemp_thread.c
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <stm32_ll_adc.h> // 必须引入底层库

LOG_MODULE_REGISTER(DIE_TEMP_TASK, LOG_LEVEL_INF);

/* 检查设备树配置 */
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
    !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "请在 .overlay 中配置 zephyr,user 节点并添加 io-channels = <&adc1 18>;"
#endif

static const struct adc_dt_spec adc_chan0 = 
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

/* 读取芯片内部温度失败，改尝试读取外部 ADC 通道 PA5 */
void dietemp_thread_entry(void *p1, void *p2, void *p3)
{
    int err;
    uint16_t buf;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    /* 1. 检查设备是否就绪 */
    if (!adc_is_ready_dt(&adc_chan0)) {
        LOG_ERR("ADC device not ready");
        return;
    }

    /* 2. 初始化通道 */
    err = adc_channel_setup_dt(&adc_chan0);
    if (err < 0) {
        LOG_ERR("Setup failed (err %d)", err);
        return;
    }

    LOG_INF("外部 ADC (PA5) 初始化成功！");

    while (1) {
        /* 3. 配置采样序列 */
        adc_sequence_init_dt(&adc_chan0, &sequence);

        /* 4. 读取原始值 */
        err = adc_read(adc_chan0.dev, &sequence);
        if (err == 0) {
            float voltage = (float)buf * 3.3f / 4095.0f;
            LOG_INF("PA5 Raw: %d, 电压: %.2f V", buf, (double)voltage);
        } else {
            LOG_WRN("读取失败 (err %d)", err);
        }

        k_msleep(1000);
    }
}

// 线程栈和定义
#define DIE_TEMP_STACK_SIZE 1024
#define DIE_TEMP_PRIORITY 13

/* 定义并启动线程，优先级设为 7 */
K_THREAD_DEFINE(dietemp_tid, DIE_TEMP_STACK_SIZE, 
                dietemp_thread_entry, NULL, NULL, NULL, 
                DIE_TEMP_PRIORITY, 0, 0);