#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "self_test.h"

/* 启用日志记录，模块名为 SELF_TEST */
LOG_MODULE_REGISTER(SELF_TEST, LOG_LEVEL_INF);

/* * 硬件定义：
 * 根据设备树和原理图，STM32F429I-DISCO 的 SDRAM 挂载在 Bank 2
 * 物理起始地址为 0xD0000000，大小为 8MB
 */
#define SDRAM_ADDR       0xD0000000
#define SDRAM_SIZE_BYTES (8 * 1024 * 1024)
#define SDRAM_SIZE_WORDS (SDRAM_SIZE_BYTES / sizeof(uint32_t))

void self_test_thread_entry(void *p1, void *p2, void *p3)
{
    /* 等待 500ms，确保内核和 FMC 外设初始化完全完成 */
    k_msleep(500);
    
    LOG_INF("SDRAM 自检线程已启动...");

    uint32_t *mem_ptr = (uint32_t *)SDRAM_ADDR;
    uint32_t error_count = 0;

    /* --- 第一阶段：地址线校验 (写入索引特征值) --- */
    /* 逻辑：在地址 N 写入数值 N。如果地址线虚焊或短路，会导致数据覆盖错乱。 */
    LOG_INF("阶段 1: 正在向 8MB 内存写入索引数据...");
    for (uint32_t i = 0; i < SDRAM_SIZE_WORDS; i++) {
        mem_ptr[i] = i; 
    }

    LOG_INF("阶段 1: 正在校验索引数据...");
    for (uint32_t i = 0; i < SDRAM_SIZE_WORDS; i++) {
        if (mem_ptr[i] != i) {
            /* 限制错误打印数量，防止日志缓冲区溢出 */
            if (error_count < 5) {
                LOG_ERR("地址错误 @ 0x%p! 期望值: 0x%08X, 实际值: 0x%08X", 
                        &mem_ptr[i], i, mem_ptr[i]);
            }
            error_count++;
        }
    }

    /* --- 第二阶段：数据线翻转测试 (0xAAAAAAAA 和 0x55555555) --- */
    /* 逻辑：通过 101010... 和 010101... 的交替，检查 16 位数据总线的每一根线是否都能正常拉高和拉低。 */
    LOG_INF("阶段 2: 正在进行位翻转测试 (0xAAAAAAAA / 0x55555555)...");
    for (uint32_t i = 0; i < SDRAM_SIZE_WORDS; i++) {
        // 测试全 1/0 交替
        mem_ptr[i] = 0xAAAAAAAA;
        if (mem_ptr[i] != 0xAAAAAAAA) error_count++;
        
        mem_ptr[i] = 0x55555555;
        if (mem_ptr[i] != 0x55555555) error_count++;
    }

    /* --- 最终结果统计 --- */
    if (error_count == 0) {
        LOG_INF(">>> [成功] SDRAM 8MB 全覆盖校验通过！硬件连接和时序配置正确。");
    } else {
        LOG_ERR(">>> [失败] SDRAM 发现 %u 处错误！请检查 FMC 引脚配置或时序参数。", error_count);
    }

    /* 测试完成后，该线程进入永久休眠，不占用 CPU 资源 */
    while (1) {
        k_sleep(K_FOREVER);
    }
}

/* --- 线程定义 (使用 Zephyr 静态定义宏) --- */
/* * 栈大小：1024 字节
 * 优先级：10 (比 LED 线程的 15 优先级更高，确保先运行测试)
 */
#define SELF_TEST_STACK_SIZE 1024
#define SELF_TEST_PRIORITY 10

K_THREAD_DEFINE(self_test_tid, SELF_TEST_STACK_SIZE,
                self_test_thread_entry, NULL, NULL, NULL,
                SELF_TEST_PRIORITY, 0, 0);