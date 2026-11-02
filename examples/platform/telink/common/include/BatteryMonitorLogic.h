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

#include "BatteryMonitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pure state machine state. It deliberately has no Zephyr dependency so that
 * it can be exercised on native_sim.
 */
typedef struct
{
    battery_level_t level;
    battery_level_t candidate;
    uint8_t candidate_count;
    uint8_t protect_count;
    bool latched;
} battery_monitor_state_t;

void battery_monitor_state_reset(battery_monitor_state_t * state, battery_level_t level);

/**
 * Compute the next level. Only adjacent transitions are allowed:
 * OK -> WARNING -> CRITICAL and back.
 */
battery_level_t battery_level_next(battery_level_t current, uint32_t mv,
                                   const battery_monitor_config_t * cfg);

/**
 * Apply offset/gain calibration to a raw millivolt reading.
 */
uint32_t battery_apply_calibration(uint32_t raw_mv, int16_t offset_mv, int32_t gain_ppm);

typedef struct
{
    uint32_t raw_mv;
    uint32_t actual_mv;
} battery_cal_point_t;

/**
 * Compute offset/gain from one or two calibration points.
 *
 * One point produces an offset-only calibration. Two points produce a
 * gain (ppm) and an offset. Returns -EINVAL for invalid points and -ERANGE
 * when the offset does not fit int16_t.
 */
int battery_calibration_compute(const battery_cal_point_t * points, size_t count, int16_t * offset_mv,
                                int32_t * gain_ppm);

/**
 * Convert a voltage to a doubled percentage (0..200) using a piecewise linear curve.
 */
uint8_t battery_percent_from_mv(uint32_t mv, const battery_curve_point_t * curve, size_t curve_size);

/**
 * Feed one (already calibrated) sample into the state machine.
 *
 * @return the action to execute, or BATTERY_ACTION_NONE. The action is returned
 *         exactly once per latch; it is not returned again until the state has
 *         recovered above protect_up_mv.
 */
battery_action_t battery_monitor_state_update(battery_monitor_state_t * state, uint32_t mv,
                                               const battery_monitor_config_t * cfg);

/**
 * Default curve used by battery_monitor_config_defaults().
 */
const battery_curve_point_t * battery_default_curve(size_t * count);

#ifdef __cplusplus
}
#endif
