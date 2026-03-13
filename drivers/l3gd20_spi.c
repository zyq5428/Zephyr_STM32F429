#define DT_DRV_COMPAT st_l3gd20

#include <zephyr/logging/log.h>
#include "l3gd20.h"

#if DT_ANY_INST_ON_BUS_STATUS_OKAY(spi)

LOG_MODULE_DECLARE(l3gd20, CONFIG_SENSOR_LOG_LEVEL);

/* 这里的函数签名现在匹配我们自定义的 l3gd20_read_ptr */
static int32_t l3gd20_read(const struct device *dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    const struct l3gd20_config *config = dev->config;
    uint8_t buffer_tx[1] = { reg | 0x80 | 0x40 }; 
    
    const struct spi_buf tx_buf = { .buf = buffer_tx, .len = 1 };
    const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };
    const struct spi_buf rx_buf[2] = {
        { .buf = NULL, .len = 1 },
        { .buf = data, .len = len }
    };
    const struct spi_buf_set rx = { .buffers = rx_buf, .count = 2 };

    return (int32_t)spi_transceive_dt(&config->spi, &tx, &rx);
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