#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(GYRO_TASK, LOG_LEVEL_INF);

/* --- 配置常量 --- */
#define RAD_TO_DPS 57.2957795f
#define FILTER_ALPHA 0.5f  // 滤波系数 (0.0~1.0)，越小越平滑，但响应变慢

/* 静态变量，用于存储平滑后的值 */
static float smooth_gx, smooth_gy, smooth_gz;

/**
 * @brief 自定义转换函数：解决 Zephyr 负数转换时的符号丢失问题
 */
static float sensor_to_float(struct sensor_value *v)
{
    /* 显式处理：整数部分 + (微量部分 / 100万) */
    /* 注意：如果 v->val1 是 0 且数值为负，符号会由驱动层处理或在此手动判定 */
    return (float)v->val1 + (float)v->val2 / 1000000.0f;
}

void gyro_thread_entry(void *p1, void *p2, void *p3)
{
    const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(l3gd20));
    struct sensor_value gyro[3];
    
    float offset_x = 0.0f, offset_y = 0.0f, offset_z = 0.0f;
    int calib_samples = 100;

    if (!device_is_ready(dev)) {
        LOG_ERR("L3GD20 未就绪");
        return;
    }

    /* --- 1. 自动校准阶段 --- */
    LOG_INF("正在校准，请保持设备静止...");
    for (int i = 0; i < calib_samples; i++) {
        if (sensor_sample_fetch(dev) == 0) {
            sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);
            offset_x += sensor_to_float(&gyro[0]);
            offset_y += sensor_to_float(&gyro[1]);
            offset_z += sensor_to_float(&gyro[2]);
        }
        k_msleep(10);
    }
    offset_x /= calib_samples;
    offset_y /= calib_samples;
    offset_z /= calib_samples;
    LOG_INF("校准完成！偏移量 -> X:%.3f, Y:%.3f, Z:%.3f", (double)offset_x, (double)offset_y, (double)offset_z);

    /* --- 2. 主循环 --- */
    while (1) {
        if (sensor_sample_fetch(dev) == 0) {
            sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);

            /* A. 获取去偏后的原始值 (单位: rad/s) */
            float cur_gx = sensor_to_float(&gyro[0]) - offset_x;
            float cur_gy = sensor_to_float(&gyro[1]) - offset_y;
            float cur_gz = sensor_to_float(&gyro[2]) - offset_z;

            /* B. 一阶低通滤波 (消除抖动) */
            smooth_gx = (cur_gx * FILTER_ALPHA) + (smooth_gx * (1.0f - FILTER_ALPHA));
            smooth_gy = (cur_gy * FILTER_ALPHA) + (smooth_gy * (1.0f - FILTER_ALPHA));
            smooth_gz = (cur_gz * FILTER_ALPHA) + (smooth_gz * (1.0f - FILTER_ALPHA));

            /* C. 转换并打印 (单位: dps) */
            LOG_INF("Gyro [dps] -> X: %6.2f | Y: %6.2f | Z: %6.2f", 
                    (double)(smooth_gx * RAD_TO_DPS), 
                    (double)(smooth_gy * RAD_TO_DPS), 
                    (double)(smooth_gz * RAD_TO_DPS));
        }
        k_msleep(100); // 10Hz 刷新率
    }
}

// 线程栈和定义
#define GYRO_STACK_SIZE 2048
#define GYRO_PRIORITY 7

/* 使用宏定义并自动启动线程 */
/* 参数：线程名, 堆栈大小, 入口函数, 参数1, 参数2, 参数3, 优先级, 选项, 延迟启动时间 */
K_THREAD_DEFINE(gyro_tid, GYRO_STACK_SIZE,
                gyro_thread_entry, NULL, NULL, NULL,
                GYRO_PRIORITY, 0, 0);
