/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Battery charge level, mirrors the Matter PowerSource BatChargeLevelEnum.
 */
typedef enum
{
    BATTERY_LEVEL_OK       = 0,
    BATTERY_LEVEL_WARNING  = 1,
    BATTERY_LEVEL_CRITICAL = 2,
} battery_level_t;

/**
 * Action executed once when the battery stays below the protection threshold.
 */
typedef enum
{
    BATTERY_ACTION_NONE = 0,
    BATTERY_ACTION_SAFE_MODE,
    BATTERY_ACTION_SHUTDOWN,
    BATTERY_ACTION_REBOOT,
} battery_action_t;

/**
 * Point of the voltage -> state-of-charge curve.
 * Values are sorted by decreasing voltage.
 */
typedef struct
{
    uint16_t mv;
    uint8_t percent_x2; /* 0.5 % unit, 0..200 */
} battery_curve_point_t;

typedef struct
{
    uint32_t voltage_mv;
    uint8_t percent_x2; /* 0.5 % unit, 0..200 */
    battery_level_t level;
    bool valid;
    uint32_t seq;
} battery_status_t;

typedef void (*battery_status_cb_t)(const battery_status_t * status, void * user_data);

typedef struct
{
    /* Sampling */
    uint32_t sample_period_ms;
    uint32_t low_sample_period_ms; /* used while level != OK */
    uint8_t confirm_samples;       /* consecutive samples needed for a level change */
    uint8_t protect_samples;       /* consecutive samples needed for the protection action */

    /* Voltage -> state of charge */
    uint16_t full_mv;
    uint16_t empty_mv;
    const battery_curve_point_t * curve;
    size_t curve_size;

    /* Reporting thresholds (enter/exit pairs) */
    uint16_t warn_down_mv;
    uint16_t warn_up_mv;
    uint16_t crit_down_mv;
    uint16_t crit_up_mv;

    /* Protection thresholds, independent from the reporting ones */
    uint16_t protect_down_mv;
    uint16_t protect_up_mv;
    battery_action_t action;

    /* Board level calibration */
    int16_t cal_offset_mv;
    int32_t cal_gain_ppm;
} battery_monitor_config_t;

/**
 * Fill @p cfg with the default configuration (Li-MnO2 3.0 V coin cell profile).
 */
void battery_monitor_config_defaults(battery_monitor_config_t * cfg);

/**
 * Initialize the service. The configuration is copied internally.
 */
int battery_monitor_init(const battery_monitor_config_t * cfg);

/**
 * Start periodic sampling. The callback is invoked for every valid sample.
 */
int battery_monitor_start(battery_status_cb_t cb, void * user_data);

/**
 * Perform one synchronous sample.
 */
int battery_monitor_read_now(battery_status_t * out);

/**
 * Change the protection action at runtime.
 */
void battery_monitor_set_action(battery_action_t action);
battery_action_t battery_monitor_get_action(void);

/**
 * Return the last produced status. Useful for late subscribers.
 */
const battery_status_t * battery_monitor_get_status(void);

/**
 * Weak hook invoked once when the protection action fires. Applications can
 * override it to enter a safe mode, shut down or reboot.
 */
void battery_monitor_action_hook(battery_action_t action);

#ifdef __cplusplus
}
#endif
