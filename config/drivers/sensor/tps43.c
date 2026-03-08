/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT azoteq_tps43

#include "tps43.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/input/input.h>

LOG_MODULE_REGISTER(tps43, CONFIG_SENSOR_LOG_LEVEL);

#if CONFIG_ZMK_SENSOR_TPS43_GESTURE_SUPPORT
// Gesture-related code would go here
#endif

/* Forward declarations */
static int tps43_device_init(const struct device *dev);
static int tps43_device_reset(const struct device *dev);
static int tps43_verify_device_id(const struct device *dev);
static int tps43_configure_device(const struct device *dev);

struct tps43_data {
    struct k_work_delayable work;
    struct k_mutex lock;
    const struct device *dev;
    struct gpio_callback gpio_cb;
    
    /* Touch data */
    int16_t x;
    int16_t y;
    int16_t last_x;
    int16_t last_y;
    bool last_touch_state;
    uint8_t touch_state;
    uint8_t touch_strength;
    
    /* Device state */
    bool device_ready;
    bool initialized;
    uint8_t error_count;
    
    /* Callbacks */
    sensor_trigger_handler_t trigger_handler;
    const struct sensor_trigger *trigger;
};

struct tps43_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec rdy_gpio;
    struct gpio_dt_spec rst_gpio;
    uint16_t resolution_x;
    uint16_t resolution_y;
    bool invert_x;
    bool invert_y;
    bool swap_xy;
};

/* I2C helper functions with error recovery */
static int tps43_i2c_read_reg(const struct device *dev, uint8_t reg, uint8_t *data, size_t len)
{
    const struct tps43_config *config = dev->config;
    int ret;
    
    ret = i2c_write_read_dt(&config->i2c, &reg, 1, data, len);
    if (ret < 0) {
        LOG_ERR("Failed to read register 0x%02x: %d", reg, ret);
        i2c_recover_bus(config->i2c.bus);
        return ret;
    }
    
    return 0;
}

static int tps43_i2c_write_reg(const struct device *dev, uint8_t reg, uint8_t *data, size_t len)
{
    const struct tps43_config *config = dev->config;
    uint8_t buffer[len + 1];
    int ret;
    
    buffer[0] = reg;
    memcpy(&buffer[1], data, len);
    
    ret = i2c_write_dt(&config->i2c, buffer, len + 1);
    if (ret < 0) {
        LOG_ERR("Failed to write register 0x%02x: %d", reg, ret);
        return ret;
    }
    
    return 0;
}

static int tps43_verify_device_id(const struct device *dev)
{
    uint8_t device_info[2];
    int ret;
    
    ret = tps43_i2c_read_reg(dev, TPS43_REG_DEVICE_INFO, device_info, 2);
    if (ret < 0) {
        LOG_ERR("Failed to read device info");
        return ret;
    }
    
    uint16_t product_id = sys_get_be16(device_info);
    LOG_INF("Device Product ID: 0x%04x", product_id);
    
    /* IQS572 should return specific product ID (58 decimal = 0x3a) */
    if (product_id != 0x003a) {
        LOG_WRN("Unexpected product ID: 0x%04x, expected: 0x003a", product_id);
        /* Don't fail completely - some variants might have different IDs */
    }
    
    return 0;
}

static int tps43_device_reset(const struct device *dev)
{
    const struct tps43_config *config = dev->config;
    struct tps43_data *data = dev->data;
    
    if (!config->rst_gpio.port) {
        return 0;
    }
    
    if (!gpio_is_ready_dt(&config->rst_gpio)) {
        LOG_WRN("Reset GPIO not ready");
        return 0;
    }
    
    LOG_INF("Performing hardware reset");
    
    /* Reset sequence: ACTIVE (Reset) -> wait -> INACTIVE (Run) */
    gpio_pin_set_dt(&config->rst_gpio, 1);
    k_msleep(20);
    gpio_pin_set_dt(&config->rst_gpio, 0);
    k_msleep(200); /* Increased delay for device to boot and stabilize */
    
    /* Reset driver state */
    data->device_ready = false;
    data->error_count = 0;
    data->x = 0;
    data->y = 0;
    data->touch_state = 0;
    
    return 0;
}

static int tps43_configure_device(const struct device *dev)
{
    const struct tps43_config *config = dev->config;
    uint8_t sys_cfg[2];
    int ret;
    
    /* Read current system configuration */
    ret = tps43_i2c_read_reg(dev, TPS43_REG_SYS_CONFIG_0, sys_cfg, 2);
    if (ret < 0) {
        LOG_ERR("Failed to read system config");
        return ret;
    }
    
    /* Configure based on DT properties */
    if (config->invert_x) {
        sys_cfg[0] |= TPS43_SYS_CFG_FLIP_X;
    }
    if (config->invert_y) {
        sys_cfg[0] |= TPS43_SYS_CFG_FLIP_Y;
    }
    if (config->swap_xy) {
        sys_cfg[0] |= TPS43_SYS_CFG_SWITCH_XY;
    }
    
    /* Acknowledge any pending reset (Bit 7 of Register 0x50) */
    sys_cfg[0] |= BIT(7); 
    
    /* System Config 1 (0x51): CLEAR bits 0 and 1 to ENABLE Touch and Prox */
    sys_cfg[1] &= ~(BIT(0) | BIT(1));
    
    ret = tps43_i2c_write_reg(dev, TPS43_REG_SYS_CONFIG_0, sys_cfg, 2);
    if (ret < 0) {
        LOG_ERR("Failed to write system config");
        return ret;
    }

    /* Verify configuration was applied */
    uint8_t read_back[2];
    if (tps43_i2c_read_reg(dev, TPS43_REG_SYS_CONFIG_0, read_back, 2) == 0) {
        LOG_INF("Config read-back: %02x %02x", read_back[0], read_back[1]);
    }

    /* Small delay for the chip to process the ACK */
    k_msleep(5);

    /* Set a more sensitive touch threshold (Register 0x41, Byte 1 is Touch) */
    uint8_t thresholds[2] = {10, 20}; /* Prox=10, Touch=20 */
    ret = tps43_i2c_write_reg(dev, TPS43_REG_THRESHOLDS, thresholds, 2);
    if (ret < 0) {
        LOG_WRN("Failed to set thresholds");
    }
    
    /* Set resolution if specified */
    if (config->resolution_x > 0 || config->resolution_y > 0) {
        uint8_t res_cfg[4];
        sys_put_be16(config->resolution_x ? config->resolution_x : 1024, &res_cfg[0]);
        sys_put_be16(config->resolution_y ? config->resolution_y : 1024, &res_cfg[2]);
        
        ret = tps43_i2c_write_reg(dev, TPS43_REG_X_RESOLUTION, res_cfg, 4);
        if (ret < 0) {
            LOG_ERR("Failed to set resolution");
            return ret;
        }
    }
    
    /* Set power mode to normal and disable event mode (for polling) */
    uint8_t pwr_mode = 0x00; /* Normal power, Stream Mode */
    ret = tps43_i2c_write_reg(dev, TPS43_REG_POWER_MODE, &pwr_mode, 1);
    if (ret < 0) {
        LOG_ERR("Failed to set power mode");
        return ret;
    }

    LOG_INF("Device configured successfully");
    return 0;
}

static int tps43_device_init(const struct device *dev)
{
    struct tps43_data *data = dev->data;
    int ret;
    
    /* Reset device first */
    ret = tps43_device_reset(dev);
    if (ret < 0) {
        LOG_ERR("Device reset failed");
        return ret;
    }
    
    /* Verify device ID */
    ret = tps43_verify_device_id(dev);
    if (ret < 0) {
        LOG_ERR("Device verification failed");
        return ret;
    }
    
    /* Configure device */
    ret = tps43_configure_device(dev);
    if (ret < 0) {
        LOG_ERR("Device configuration failed");
        return ret;
    }
    
    data->device_ready = true;
    data->initialized = true;
    
    LOG_INF("TPS43 device initialized successfully");
    return 0;
}

static int tps43_read_touch_data(const struct device *dev)
{
    struct tps43_data *data = dev->data;
    uint8_t touch_data[8];
    int ret;
    
    if (!data->device_ready) {
        return -ENODEV;
    }

    /* Read XY info and coordinates in one transaction */
    ret = tps43_i2c_read_reg(dev, TPS43_REG_XY_INFO_0, touch_data, 8);
    if (ret < 0) {
        // Only log error occasionally to avoid spamming
        if (data->error_count == 0) {
            LOG_ERR("Failed to read touch data");
        }
        data->error_count++;
        if (data->error_count > TPS43_MAX_ERROR_COUNT) {
            LOG_WRN("Too many errors, reinitializing device");
            data->device_ready = false;
            k_work_reschedule(&data->work, K_MSEC(1500));
        }
        return ret;
    }
    
    /* Reset error count on successful read */
    data->error_count = 0;
    
    /* Log some raw data every 50 polls even if zero, just to see heartbeat */
    static uint8_t heartbeat_counter = 0;
    if (++heartbeat_counter >= 50) {
        heartbeat_counter = 0;
        const struct tps43_config *config = dev->config;
        int rdy_state = gpio_pin_get_dt(&config->rdy_gpio);
        LOG_INF("Heartbeat - Raw: %02x %02x %02x %02x, RDY: %d", 
                touch_data[0], touch_data[1], touch_data[2], touch_data[3], rdy_state);
    }

    /* Parse touch data */
    uint8_t xy_info = touch_data[0];
    
    if (xy_info != 0) {
        LOG_INF("Raw XY_INFO: 0x%02x", xy_info);
    }

    /* Check for reset bit (bit 7) */
    if (xy_info & 0x80) {
        LOG_INF("Device reset bit detected, acknowledging...");
        /* We'll acknowledge this during the next configuration or via a specific write */
        uint8_t ack_val = 0x80;
        tps43_i2c_write_reg(dev, TPS43_REG_SYS_CONFIG_0, &ack_val, 1);
    }

    data->touch_state = (xy_info & TPS43_XY_INFO_TOUCH_MASK) ? 1 : 0;
    
    if (data->touch_state) {
        /* Extract coordinates (Big Endian) */
        data->x = sys_get_be16(&touch_data[4]);
        data->y = sys_get_be16(&touch_data[6]);
        data->touch_strength = touch_data[2];
        
        LOG_INF("Touch detected: x=%d, y=%d, strength=%d", data->x, data->y, data->touch_strength);

        /* Report relative movement if this is a continuation of a touch */
        if (data->last_touch_state) {
            int16_t dx = data->x - data->last_x;
            int16_t dy = data->y - data->last_y;
            
            if (dx != 0 || dy != 0) {
                input_report_rel(dev, INPUT_REL_X, dx, false, K_FOREVER);
                input_report_rel(dev, INPUT_REL_Y, dy, true, K_FOREVER);
            }
        }
        
        data->last_x = data->x;
        data->last_y = data->y;
    } else if (data->last_touch_state) {
        LOG_INF("Touch released");
    }
    
    data->last_touch_state = data->touch_state;
    
    return 0;
}

static void tps43_work_handler(struct k_work *work)
{
    struct k_work_delayable *delayable_work = k_work_delayable_from_work(work);
    struct tps43_data *data = CONTAINER_OF(delayable_work, struct tps43_data, work);
    const struct device *dev = data->dev;
    int ret;
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    /* Reinitialize device if needed */
    if (!data->device_ready) {
        LOG_INF("Reinitializing device");
        ret = tps43_device_init(dev);
        if (ret < 0) {
            LOG_ERR("Device reinitialization failed");
            k_mutex_unlock(&data->lock);
            /* Retry after longer delay */
            k_work_reschedule(&data->work, K_MSEC(1000));
            return;
        }
    }
    
    /* Read touch data */
    ret = tps43_read_touch_data(dev);
    if (ret == 0) {
        LOG_INF("Poll successful, touch_state: %d", data->touch_state);
    } else {
        LOG_WRN("Touch read failed: %d", ret);
    }
    
    /* Trigger callback if configured */
    if (data->trigger_handler && data->trigger) {
        data->trigger_handler(dev, data->trigger);
    }

    /* Reschedule for polling if configured - ALWAYS reschedule so loop doesn't die */
#if CONFIG_ZMK_SENSOR_TPS43_POLL_RATE_MS > 0
    k_work_reschedule(&data->work, K_MSEC(CONFIG_ZMK_SENSOR_TPS43_POLL_RATE_MS));
#endif
    
    k_mutex_unlock(&data->lock);
}

static void tps43_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_DBG("RDY interrupt fired");
    struct tps43_data *data = CONTAINER_OF(cb, struct tps43_data, gpio_cb);
    k_work_reschedule(&data->work, K_NO_WAIT);
}

static int tps43_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct tps43_data *data = dev->data;
    int ret;
    
    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_POS_DX && chan != SENSOR_CHAN_POS_DY) {
        return -ENOTSUP;
    }
    
    k_mutex_lock(&data->lock, K_FOREVER);
    ret = tps43_read_touch_data(dev);
    k_mutex_unlock(&data->lock);
    
    return ret;
}

static int tps43_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct tps43_data *data = dev->data;
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    switch (chan) {
    case SENSOR_CHAN_POS_DX:
        val->val1 = data->x;
        val->val2 = 0;
        break;
    case SENSOR_CHAN_POS_DY:
        val->val1 = data->y;
        val->val2 = 0;
        break;
    default:
        k_mutex_unlock(&data->lock);
        return -ENOTSUP;
    }
    
    k_mutex_unlock(&data->lock);
    return 0;
}

static int tps43_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
                           sensor_trigger_handler_t handler)
{
    struct tps43_data *data = dev->data;
    const struct tps43_config *config = dev->config;
    int ret = 0;
    
    k_mutex_lock(&data->lock, K_FOREVER);
    
    if (trig->type != SENSOR_TRIG_DATA_READY) {
        ret = -ENOTSUP;
        goto unlock;
    }
    
    data->trigger_handler = handler;
    data->trigger = trig;
    
    if (handler) {
        /* Enable interrupt */
        ret = gpio_pin_interrupt_configure_dt(&config->rdy_gpio, GPIO_INT_EDGE_FALLING);
        LOG_INF("RDY interrupt configured: %d", ret);
        if (ret < 0) {
                LOG_ERR("Failed to enable RDY interrupt: %d", ret);
            goto unlock;
        }
    } else {
        /* Disable interrupt */
        ret = gpio_pin_interrupt_configure_dt(&config->rdy_gpio, GPIO_INT_DISABLE);
    }
    
unlock:
    k_mutex_unlock(&data->lock);
    return ret;
}

static const struct sensor_driver_api tps43_driver_api = {
    .sample_fetch = tps43_sample_fetch,
    .channel_get = tps43_channel_get,
    .trigger_set = tps43_trigger_set,
};

static int tps43_init(const struct device *dev)
{
    struct tps43_data *data = dev->data;
    const struct tps43_config *config = dev->config;
    int ret;
    
    data->dev = dev;
    k_mutex_init(&data->lock);
    k_work_init_delayable(&data->work, tps43_work_handler);
    
    if (!i2c_is_ready_dt(&config->i2c)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }
    
    if (gpio_is_ready_dt(&config->rst_gpio)) {
        ret = gpio_pin_configure_dt(&config->rst_gpio, GPIO_OUTPUT_ACTIVE);
        if (ret < 0) {
            LOG_WRN("Failed to configure reset GPIO: %d", ret);
        }
    }
    
    if (gpio_is_ready_dt(&config->rdy_gpio)) {
        LOG_INF("RDY GPIO is ready, configuring interrupt");
        ret = gpio_pin_configure_dt(&config->rdy_gpio, GPIO_INPUT);
        if (ret < 0) {
            LOG_WRN("Failed to configure RDY GPIO: %d", ret);
        } else {
            gpio_init_callback(&data->gpio_cb, tps43_gpio_callback, BIT(config->rdy_gpio.pin));
            ret = gpio_add_callback(config->rdy_gpio.port, &data->gpio_cb);
            if (ret < 0) {
                LOG_WRN("Failed to add GPIO callback: %d", ret);
            }
            ret = gpio_pin_interrupt_configure_dt(&config->rdy_gpio, GPIO_INT_EDGE_FALLING);
            LOG_INF("RDY interrupt configured: %d", ret);
        }
    } else {
        LOG_WRN("RDY GPIO not ready - interrupt mode unavailable");
    }
    
    /* Non-blocking init - schedule device init as work item instead */
    k_work_reschedule(&data->work, K_MSEC(500));
    
    LOG_INF("TPS43 driver registered, init scheduled");
    return 0;
}

#define TPS43_DEFINE(inst)                                                    \
    static struct tps43_data tps43_data_##inst;                             \
                                                                             \
    static const struct tps43_config tps43_config_##inst = {                \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                 \
        .rdy_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, rdy_gpios, {0}),        \
        .rst_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, rst_gpios, {0}),        \
        .resolution_x = DT_INST_PROP_OR(inst, resolution_x, 0),            \
        .resolution_y = DT_INST_PROP_OR(inst, resolution_y, 0),            \
        .invert_x = DT_INST_PROP(inst, invert_x),                          \
        .invert_y = DT_INST_PROP(inst, invert_y),                          \
        .swap_xy = DT_INST_PROP(inst, swap_xy),                            \
    };                                                                       \
                                                                             \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, tps43_init, NULL,                   \
                                &tps43_data_##inst, &tps43_config_##inst,   \
                                POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,    \
                                &tps43_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TPS43_DEFINE) 