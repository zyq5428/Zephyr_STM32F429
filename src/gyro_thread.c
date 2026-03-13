#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

// 启用日志记录
LOG_MODULE_REGISTER(GYRO_TASK, LOG_LEVEL_INF);

/* 声明全局变量，用于在不同线程间传递数据 (简单示例) */
static float gx, gy, gz;

/**
 * @brief L3GD20 数据读取线程函数
 * * 此函数将被分配到一个独立的线程中运行。
 */
void gyro_thread_entry(void *p1, void *p2, void *p3)
{
    /* 1. 获取设备实例 */
    /* DT_NODELABEL(l3gd20) 对应你设备树中的标签 */
    const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(l3gd20));
    struct sensor_value gyro[3];

    /* 2. 检查设备是否准备就绪 */
    if (!device_is_ready(dev)) {
        LOG_WRN("错误: L3GD20 设备未就绪！\n");
        return;
    }

    LOG_INF("L3GD20 读取线程已启动...\n");

    /* 3. 循环读取数据 */
    while (1) {
        /* 触发传感器采样（从硬件读取原始值） */
        if (sensor_sample_fetch(dev) < 0) {
            LOG_WRN("警告: 无法获取陀螺仪样本数据\n");
        } else {
            /* 获取转换后的角速度值（单位：rad/s） */
            sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ, gyro);

            /* 将结果转换为浮点数并存入全局变量 */
            /* 注意：sensor_value_to_double 是 Zephyr 提供的便捷转换函数 */
            gx = (float)sensor_value_to_double(&gyro[0]);
            gy = (float)sensor_value_to_double(&gyro[1]);
            gz = (float)sensor_value_to_double(&gyro[2]);

            /* 打印数据（调试用） */
            /* 如果你想看度数/秒 (dps)，请将弧度乘以 57.295 */
            /* 在 Zephyr 日志中使用 (double) 进行强转是消除此类警告的标准做法 */
            LOG_INF("Gyro [rad/s] -> X: %.3f, Y: %.3f, Z: %.3f", 
                    (double)gx, (double)gy, (double)gz);
            }

        /* 4. 线程睡眠，控制采样频率（例如 10Hz = 100ms） */
        k_msleep(100);
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
