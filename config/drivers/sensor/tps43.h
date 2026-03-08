/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_TPS43_H_
#define ZEPHYR_DRIVERS_SENSOR_TPS43_H_

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

/* TPS43 with IQS572 controller register definitions (16-bit addresses) */
#define TPS43_REG_DEVICE_INFO       0x0000
#define TPS43_REG_SYS_INFO_0        0x0001
#define TPS43_REG_SYS_INFO_1        0x0002
#define TPS43_REG_VERSION_INFO      0x0003
#define TPS43_REG_XY_INFO_0         0x0010
#define TPS43_REG_XY_INFO_1         0x0011
#define TPS43_REG_TOUCH_STRENGTH    0x0012
#define TPS43_REG_TOUCH_AREA        0x0013
#define TPS43_REG_COORDINATES_X     0x0014
#define TPS43_REG_COORDINATES_Y     0x0016
#define TPS43_REG_PROX_STATUS       0x0020
#define TPS43_REG_TOUCH_STATUS      0x0021
#define TPS43_REG_COUNTS            0x0022
#define TPS43_REG_LTA               0x0023
#define TPS43_REG_DELTAS            0x0024
#define TPS43_REG_MULTIPLIERS       0x0025
#define TPS43_REG_COMPENSATION      0x0026
#define TPS43_REG_PROX_SETTINGS     0x0040
#define TPS43_REG_THRESHOLDS        0x0041
#define TPS43_REG_TIMINGS_0         0x0042
#define TPS43_REG_TIMINGS_1         0x0043
#define TPS43_REG_GESTURE_TIMERS    0x0044
#define TPS43_REG_ACTIVE_CHANNELS   0x0045

/* IQS572 specific configuration registers */
#define TPS43_REG_SYS_CONFIG        0x0050
#define TPS43_REG_FILTER_SETTINGS   0x0051
#define TPS43_REG_POWER_MODE        0x0052

/* IQS572 specific values for TPS43 */
#define TPS43_MAX_X                 2048
#define TPS43_MAX_Y                 1792
#define TPS43_MAX_TOUCH_POINTS      5

/* IQS572 device ID */
#define TPS43_DEVICE_ID             0x41  /* Expected device ID for IQS572 */

/* IQS572 system configuration flags */
#define TPS43_SYS_CFG_FLIP_X        BIT(0)
#define TPS43_SYS_CFG_FLIP_Y        BIT(1)
#define TPS43_SYS_CFG_SWITCH_XY     BIT(2)
#define TPS43_SYS_CFG_TP_EVENT      BIT(3)
#define TPS43_SYS_CFG_PROX_EVENT    BIT(4)

/* Touch detection flags */
#define TPS43_XY_INFO_TOUCH_MASK    0x01

/* IQS572 expected product ID (58 decimal = 0x3a) */
#define TPS43_EXPECTED_PRODUCT_ID   0x3a  /* Expected device ID for IQS572 */

/* Error recovery thresholds */
#define TPS43_MAX_ERROR_COUNT       5

/* Device state flags */
#define TPS43_SHOW_RESET            BIT(7)
#define TPS43_TOUCH_EVENT           BIT(0)
#define TPS43_PROX_EVENT            BIT(1)

/* Communication timeouts and retry counts */
#define TPS43_I2C_TIMEOUT_MS        500
#define TPS43_INIT_TIMEOUT_MS       1000
#define TPS43_MAX_RETRIES           3

#define TPS43_REG_SYS_CONFIG_0      0x0050
#define TPS43_REG_X_RESOLUTION      0x0053

#endif /* ZEPHYR_DRIVERS_SENSOR_TPS43_H_ */ 