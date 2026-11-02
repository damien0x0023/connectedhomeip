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

#include <errno.h>

#include <zephyr/ztest.h>

#include "BatteryMonitorLogic.h"

static battery_monitor_config_t make_config(void)
{
    battery_monitor_config_t cfg;

    battery_monitor_config_defaults(&cfg);
    return cfg;
}

ZTEST(battery_monitor_logic, test_defaults)
{
    battery_monitor_config_t cfg = make_config();

    zassert_equal(cfg.full_mv, 3000);
    zassert_equal(cfg.empty_mv, 2100);
    zassert_equal(cfg.warn_down_mv, 2600);
    zassert_equal(cfg.crit_down_mv, 2300);
    zassert_equal(cfg.protect_down_mv, 2150);
    zassert_equal(cfg.confirm_samples, 3);
    zassert_equal(cfg.protect_samples, 5);
    zassert_not_null(cfg.curve);
    zassert_true(cfg.curve_size > 2);
}

ZTEST(battery_monitor_logic, test_level_ladder)
{
    battery_monitor_config_t cfg = make_config();

    zassert_equal(battery_level_next(BATTERY_LEVEL_OK, 2601, &cfg), BATTERY_LEVEL_OK);
    zassert_equal(battery_level_next(BATTERY_LEVEL_OK, 2600, &cfg), BATTERY_LEVEL_WARNING);

    zassert_equal(battery_level_next(BATTERY_LEVEL_WARNING, 2301, &cfg), BATTERY_LEVEL_WARNING);
    zassert_equal(battery_level_next(BATTERY_LEVEL_WARNING, 2300, &cfg), BATTERY_LEVEL_CRITICAL);
    zassert_equal(battery_level_next(BATTERY_LEVEL_WARNING, 2799, &cfg), BATTERY_LEVEL_WARNING);
    zassert_equal(battery_level_next(BATTERY_LEVEL_WARNING, 2800, &cfg), BATTERY_LEVEL_OK);

    zassert_equal(battery_level_next(BATTERY_LEVEL_CRITICAL, 2499, &cfg), BATTERY_LEVEL_CRITICAL);
    zassert_equal(battery_level_next(BATTERY_LEVEL_CRITICAL, 2500, &cfg), BATTERY_LEVEL_WARNING);

    /* A critical battery must never jump straight back to OK. */
    zassert_equal(battery_level_next(BATTERY_LEVEL_CRITICAL, 3000, &cfg), BATTERY_LEVEL_WARNING);
}

ZTEST(battery_monitor_logic, test_confirm_samples)
{
    battery_monitor_config_t cfg = make_config();
    battery_monitor_state_t state;

    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    zassert_equal(battery_monitor_state_update(&state, 2600, &cfg), BATTERY_ACTION_NONE);
    zassert_equal(state.level, BATTERY_LEVEL_OK);
    zassert_equal(battery_monitor_state_update(&state, 2600, &cfg), BATTERY_ACTION_NONE);
    zassert_equal(state.level, BATTERY_LEVEL_OK);
    zassert_equal(battery_monitor_state_update(&state, 2600, &cfg), BATTERY_ACTION_NONE);
    zassert_equal(state.level, BATTERY_LEVEL_WARNING);
}

ZTEST(battery_monitor_logic, test_protection_latches_once)
{
    battery_monitor_config_t cfg = make_config();
    battery_monitor_state_t state;

    cfg.action = BATTERY_ACTION_SAFE_MODE;
    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    for (int i = 0; i < cfg.protect_samples - 1; i++)
    {
        zassert_equal(battery_monitor_state_update(&state, 2100, &cfg), BATTERY_ACTION_NONE);
    }

    zassert_equal(battery_monitor_state_update(&state, 2100, &cfg), BATTERY_ACTION_SAFE_MODE);
    zassert_true(state.latched);

    /* Same sample again: the action must not repeat and the counter must stay put. */
    uint8_t protect_count = state.protect_count;

    zassert_equal(battery_monitor_state_update(&state, 2100, &cfg), BATTERY_ACTION_NONE);
    zassert_equal(state.protect_count, protect_count);
}

ZTEST(battery_monitor_logic, test_protection_independent_of_level)
{
    battery_monitor_config_t cfg = make_config();
    battery_monitor_state_t state;

    cfg.action = BATTERY_ACTION_SHUTDOWN;
    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    /* Boot with a flat battery: protection must fire without waiting for the ladder. */
    for (int i = 0; i < cfg.protect_samples; i++)
    {
        battery_action_t action = battery_monitor_state_update(&state, 2000, &cfg);

        if (i < cfg.protect_samples - 1)
        {
            zassert_equal(action, BATTERY_ACTION_NONE);
        }
        else
        {
            zassert_equal(action, BATTERY_ACTION_SHUTDOWN);
        }
    }
}

ZTEST(battery_monitor_logic, test_protection_recovers)
{
    battery_monitor_config_t cfg = make_config();
    battery_monitor_state_t state;

    cfg.action = BATTERY_ACTION_SAFE_MODE;
    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    for (int i = 0; i < cfg.protect_samples; i++)
    {
        (void) battery_monitor_state_update(&state, 2100, &cfg);
    }
    zassert_true(state.latched);

    zassert_equal(battery_monitor_state_update(&state, cfg.protect_up_mv, &cfg), BATTERY_ACTION_NONE);
    zassert_false(state.latched);

    /* Falling again must trigger a new action. */
    battery_action_t action = BATTERY_ACTION_NONE;
    for (int i = 0; i < cfg.protect_samples; i++)
    {
        action = battery_monitor_state_update(&state, 2100, &cfg);
    }
    zassert_equal(action, BATTERY_ACTION_SAFE_MODE);
}

ZTEST(battery_monitor_logic, test_percent_curve)
{
    battery_monitor_config_t cfg = make_config();

    zassert_equal(battery_percent_from_mv(3200, cfg.curve, cfg.curve_size), 200);
    zassert_equal(battery_percent_from_mv(3000, cfg.curve, cfg.curve_size), 200);
    zassert_equal(battery_percent_from_mv(2900, cfg.curve, cfg.curve_size), 180);
    zassert_equal(battery_percent_from_mv(2600, cfg.curve, cfg.curve_size), 90);
    zassert_equal(battery_percent_from_mv(2550, cfg.curve, cfg.curve_size), 75);
    zassert_equal(battery_percent_from_mv(2500, cfg.curve, cfg.curve_size), 60);
    zassert_equal(battery_percent_from_mv(2200, cfg.curve, cfg.curve_size), 8);
    zassert_equal(battery_percent_from_mv(2100, cfg.curve, cfg.curve_size), 0);
    zassert_equal(battery_percent_from_mv(2000, cfg.curve, cfg.curve_size), 0);

    /* NULL curve falls back to the built-in one. */
    zassert_equal(battery_percent_from_mv(2500, NULL, 0), 60);
}

ZTEST(battery_monitor_logic, test_calibration)
{
    zassert_equal(battery_apply_calibration(3300, 0, 0), 3300);
    zassert_equal(battery_apply_calibration(3300, -20, 0), 3280);
    zassert_equal(battery_apply_calibration(3300, 0, 10000), 3333);
    zassert_equal(battery_apply_calibration(1000, -2000, 0), 0);
    zassert_equal(battery_apply_calibration(4000, 500, 0), 4500);
}

ZTEST(battery_monitor_logic, test_zero_confirm_and_protect_are_clamped)
{
    battery_monitor_config_t cfg = make_config();
    battery_monitor_state_t state;

    cfg.confirm_samples = 0;
    cfg.protect_samples = 0;
    cfg.action          = BATTERY_ACTION_SAFE_MODE;
    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    /* confirm_samples = 0 is clamped to 1, so the first sample moves one step. */
    zassert_equal(battery_monitor_state_update(&state, 2100, &cfg), BATTERY_ACTION_SAFE_MODE);
    zassert_equal(state.level, BATTERY_LEVEL_WARNING);

    /* The ladder still needs two more steps to reach CRITICAL. */
    (void) battery_monitor_state_update(&state, 2100, &cfg);
    (void) battery_monitor_state_update(&state, 2100, &cfg);
    zassert_equal(state.level, BATTERY_LEVEL_CRITICAL);
}

ZTEST(battery_monitor_logic, test_calibration_two_points)
{
    battery_cal_point_t points[] = { { 3000, 3100 }, { 2400, 2480 } };
    int16_t offset                = 0;
    int32_t gain                  = 0;

    zassert_equal(battery_calibration_compute(points, 2, &offset, &gain), 0);
    zassert_equal(gain, 33333);
    zassert_equal(offset, 1);
    zassert_equal(battery_apply_calibration(2400, offset, gain), 2480);
    zassert_equal(battery_apply_calibration(3000, offset, gain), 3100);
}

ZTEST(battery_monitor_logic, test_calibration_one_point)
{
    battery_cal_point_t points[] = { { 3000, 3050 } };
    int16_t offset                = 0;
    int32_t gain                  = 0;

    zassert_equal(battery_calibration_compute(points, 1, &offset, &gain), 0);
    zassert_equal(offset, 50);
    zassert_equal(gain, 0);
    zassert_equal(battery_apply_calibration(3000, offset, gain), 3050);
}

ZTEST(battery_monitor_logic, test_calibration_invalid)
{
    battery_cal_point_t flat[] = { { 3000, 3000 }, { 3000, 3100 } };
    int16_t offset             = 0;
    int32_t gain               = 0;

    zassert_equal(battery_calibration_compute(flat, 2, &offset, &gain), -EINVAL);
    zassert_equal(battery_calibration_compute(flat, 0, &offset, &gain), -EINVAL);
}

ZTEST_SUITE(battery_monitor_logic, NULL, NULL, NULL, NULL, NULL);
