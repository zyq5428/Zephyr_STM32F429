#define DT_DRV_COMPAT st_l3gd20

#include <zephyr/logging/log.h>
#include "l3gd20.h"

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)

LOG_MODULE_DECLARE(l3gd20, CONFIG_SENSOR_LOG_LEVEL);

/* 这里的函数签名现在匹配我们自定义的 l3gd20_read_ptr */
/**
 * @brief 从 L3GD20 寄存器读取数据
 * * @param dev   设备句柄
 * @param reg   起始寄存器地址
 * @param data  存放读取数据的缓冲区指针
 * @param len   要读取的字节数
 * * @return int32_t 0表示成功，负数表示失败
 */
static int32_t l3gd20_read(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    // 1. 获取设备配置信息（内含 SPI 配置规格）
    const struct l3gd20_config *config = dev->config;
    int ret;

    /* * 2. 构建 SPI 指令字节 (Header)
     * Bit 7: 1 (表示 READ 操作)
     * Bit 6: MS (Multiple Sample). 如果 len > 1 则设为 1，芯片会自动增加寄存器地址
     */
    uint8_t cmd = reg | 0x80; 
    if (len > 1) {
        cmd |= 0x40; 
    }
    
    // 发送缓冲区：只包含 1 个指令字节
    uint8_t buffer_tx[1] = { cmd };
    const struct spi_buf tx_buf = {
        .buf = buffer_tx,
        .len = 1
    };
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };

    /* * 3. 构建接收缓冲区 (RX)
     * SPI 是同步交换的，发送 1 字节指令的同时会收到 1 字节垃圾数据。
     * rx_buf[0]: 长度为 1，buf 为 NULL，表示丢弃掉发送指令期间收到的那个字节。
     * rx_buf[1]: 真正的数据接收区，长度为 len。
     */
    const struct spi_buf rx_buf[2] = {
        {
            .buf = NULL,
            .len = 1,
        },
        {
            .buf = data,
            .len = len,
        }
    };
    const struct spi_buf_set rx = {
        .buffers = rx_buf,
        .count = 2
    };

    // 4. 调用 Zephyr SPI 传输 API
    // 它会自动控制 CS 引脚的拉低和拉高
    ret = spi_transceive_dt(&config->spi, &tx, &rx);
    
    if (ret < 0) {
        // 如果底层 SPI 传输失败，返回错误码
        return ret;
    }

    return 0;
}

static int32_t l3gd20_write(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    const struct l3gd20_config *config = dev->config;
    uint8_t buffer_tx[1] = { reg | 0x40 };
    
    const struct spi_buf tx_buf[2] = {
        { .buf = buffer_tx, .len = 1 },
        { .buf = data, .len = len }
    };
    const struct spi_buf_set tx = { .buffers = tx_buf, .count = 2 };

    return (int32_t)spi_write_dt(&config->spi, &tx);
}

int l3gd20_spi_init(const struct device *dev)
{
    struct l3gd20_data *data = dev->data;
    const struct l3gd20_config *cfg = dev->config;

    if (!spi_is_ready_dt(&cfg->spi)) {
        LOG_ERR("SPI bus not ready");
        return -ENODEV;
    }

    /* 绑定到我们自定义的 ctx */
    data->ctx.read_reg = l3gd20_read;
    data->ctx.write_reg = l3gd20_write;

    return 0;
}

#endif