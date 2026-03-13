#define DT_DRV_COMPAT st_l3gd20

#include <zephyr/init.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "l3gd20.h"

LOG_MODULE_REGISTER(l3gd20, CONFIG_SENSOR_LOG_LEVEL);

/* 内部逻辑：使用自定义 ctx 进行寄存器操作 */
static int32_t l3gd20_read_reg(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    struct l3gd20_data *drv_data = dev->data;
    return drv_data->ctx.read_reg(dev, reg, data, len);
}

static int32_t l3gd20_write_reg(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    struct l3gd20_data *drv_data = dev->data;
    return drv_data->ctx.write_reg(dev, reg, data, len);
}

static int l3gd20_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct l3gd20_data *data = dev->data;
    uint8_t buff[6];
    int32_t ret;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_GYRO_XYZ) return -ENOTSUP;

    /* 直接读取 6 个字节获取 X, Y, Z 三轴原始值 */
    ret = l3gd20_read_reg(dev, L3GD20_OUT_X_L, buff, 6);
    if (ret == 0) {
        // 专门看 X 轴的两个原始字节
        // LOG_INF("RAW HEX -> L: 0x%02x, H: 0x%02x", buff[0], buff[1]);
        data->angular_rate[0] = (int16_t)((uint16_t)buff[1] << 8 | buff[0]);
        data->angular_rate[1] = (int16_t)((uint16_t)buff[3] << 8 | buff[2]);
        data->angular_rate[2] = (int16_t)((uint16_t)buff[5] << 8 | buff[4]);
    }
    return ret;
}

static int l3gd20_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct l3gd20_data *data = dev->data;

    if (chan != SENSOR_CHAN_GYRO_XYZ && chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }

    for (int i = 0; i < 3; i++) {
        /* 使用 int64 确保中间乘法不溢出 */
        int64_t micro_dps = (int64_t)data->angular_rate[i] * L3GD20_SENSITIVITY;

        /* 分离整数和微秒部分 */
        val[i].val1 = (int32_t)(micro_dps / 1000000LL);
        
        /* 关键：Zephyr 的 val2 必须是正数 */
        int32_t remainder = (int32_t)(micro_dps % 1000000LL);
        if (remainder < 0) {
            // 如果是负数，我们需要让 val1 向负方向进位，或者保持 val2 为正
            // 这里采用最通用的做法：val2 取绝对值
            val[i].val2 = -remainder;
        } else {
            val[i].val2 = remainder;
        }
    }
    return 0;
}

static DEVICE_API(sensor, l3gd20_api) = {
    .sample_fetch = l3gd20_sample_fetch,
    .channel_get = l3gd20_channel_get,
};

static int l3gd20_init(const struct device *dev)
{
    uint8_t wai;
    int ret;

    ret = l3gd20_spi_init(dev);
    if (ret != 0) return ret;

    /* 确认通信 */
    ret = l3gd20_read_reg(dev, L3GD20_WHO_AM_I, &wai, 1);
    if (ret != 0 || (wai != L3GD20_ID && wai != L3GD20H_ID)) {
        return -EIO;
    }

    /* 关键配置组合 */
    // CTRL1: 100Hz 采样, 开启三轴, 正常工作模式
    uint8_t ctrl1 = 0x0F; 
    l3gd20_write_reg(dev, L3GD20_CTRL_REG1, &ctrl1, 1);

    // CTRL4: 开启 BDU (非常重要!), 量程 250dps
    uint8_t ctrl4 = 0x80; 
    l3gd20_write_reg(dev, L3GD20_CTRL_REG4, &ctrl4, 1);

    LOG_INF("L3GD20 配置已刷新：BDU已开启，量程250dps");
    return 0;
}

#define L3GD20_INST(inst)                                           \
    static struct l3gd20_data l3gd20_data_##inst;                   \
    static const struct l3gd20_config l3gd20_config_##inst = {      \
        .spi = SPI_DT_SPEC_INST_GET(inst,                           \
            SPI_OP_MODE_MASTER | SPI_MODE_CPOL |                    \
            SPI_MODE_CPHA | SPI_WORD_SET(8))                     \
    };                                                              \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, l3gd20_init, NULL,           \
                                 &l3gd20_data_##inst,               \
                                 &l3gd20_config_##inst,             \
                                 POST_KERNEL,                       \
                                 CONFIG_SENSOR_INIT_PRIORITY,       \
                                 &l3gd20_api);

DT_INST_FOREACH_STATUS_OKAY(L3GD20_INST)