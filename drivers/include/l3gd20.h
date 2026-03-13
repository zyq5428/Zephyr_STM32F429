/*
 * Copyright (c) 2026 Gemini AI Collaborator
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ST_L3GD20_H_
#define ST_L3GD20_H_

#include <zephyr/types.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/sensor.h>

/* --- L3GD20 寄存器映射 --- */
#define L3GD20_WHO_AM_I         0x0F
#define L3GD20_CTRL_REG1        0x20
#define L3GD20_CTRL_REG4        0x23
#define L3GD20_STATUS_REG       0x27
#define L3GD20_OUT_X_L          0x28

/* --- 配置常量 --- */
#define L3GD20_ID               0xD4
#define L3GD20H_ID              0xD7
#define L3GD20_SENSITIVITY      8750  // 8.75 mdps/LSB

/* 自定义读写函数指针类型，取代 stmdev_ctx_t */
typedef int32_t (*l3gd20_read_ptr)(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len);
typedef int32_t (*l3gd20_write_ptr)(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len);

/* 传感器上下文 */
struct l3gd20_ctx {
    l3gd20_read_ptr read_reg;
    l3gd20_write_ptr write_reg;
};

/* 硬件配置结构 */
struct l3gd20_config {
    struct spi_dt_spec spi;
};

/* 运行数据结构 */
struct l3gd20_data {
    int16_t angular_rate[3];
    struct l3gd20_ctx ctx; // 使用我们自己的上下文结构
};

int l3gd20_spi_init(const struct device *dev);

#endif /* ST_L3GD20_H_ */