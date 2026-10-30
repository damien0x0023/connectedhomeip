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

#include "BatteryMonitorLogic.h"

#include <limits.h>

/* Li-MnO2 3.0 V coin cell, doubled percentage (0.5 % unit). */
static const battery_curve_point_t kDefaultCurve[] = {
    { 3000, 200 }, { 2900, 180 }, { 2800, 150 }, { 2700, 120 }, { 2600, 90 },
    { 2500, 60 },  { 2400, 40 },  { 2300, 20 },  { 2200, 8 },   { 2100, 0 },
};

const battery_curve_point_t * battery_default_curve(size_t * count)
{
    if (count != NULL)
    {
        *count = sizeof(kDefaultCurve) / sizeof(kDefaultCurve[0]);
    }
    return kDefaultCurve;
}

void battery_monitor_config_defaults(battery_monitor_config_t * cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->sample_period_ms     = 60000;
    cfg->low_sample_period_ms = 10000;
    cfg->confirm_samples      = 3;
    cfg->protect_samples      = 5;

    cfg->full_mv  = 3000;
    cfg->empty_mv = 2100;
    cfg->curve    = kDefaultCurve;
    cfg->curve_size = sizeof(kDefaultCurve) / sizeof(kDefaultCurve[0]);

    cfg->warn_down_mv = 2600;
    cfg->warn_up_mv   = 2800;
    cfg->crit_down_mv = 2300;
    cfg->crit_up_mv   = 2500;

    cfg->protect_down_mv = 2150;
    cfg->protect_up_mv   = 2500;
    cfg->action          = BATTERY_ACTION_NONE;

    cfg->cal_offset_mv = 0;
    cfg->cal_gain_ppm  = 0;
}

void battery_monitor_state_reset(battery_monitor_state_t * state, battery_level_t level)
{
    if (state == NULL)
    {
        return;
    }

    state->level           = level;
    state->candidate       = level;
    state->candidate_count = 0;
    state->protect_count   = 0;
    state->latched         = false;
}

battery_level_t battery_level_next(battery_level_t current, uint32_t mv,
                                   const battery_monitor_config_t * cfg)
{
    switch (current)
    {
    case BATTERY_LEVEL_OK:
        return (mv <= cfg->warn_down_mv) ? BATTERY_LEVEL_WARNING : BATTERY_LEVEL_OK;

    case BATTERY_LEVEL_WARNING:
        if (mv <= cfg->crit_down_mv)
        {
            return BATTERY_LEVEL_CRITICAL;
        }
        if (mv >= cfg->warn_up_mv)
        {
            return BATTERY_LEVEL_OK;
        }
        return BATTERY_LEVEL_WARNING;

    case BATTERY_LEVEL_CRITICAL:
        if (mv >= cfg->crit_up_mv)
        {
            return BATTERY_LEVEL_WARNING;
        }
        return BATTERY_LEVEL_CRITICAL;

    default:
        return BATTERY_LEVEL_OK;
    }
}

uint32_t battery_apply_calibration(uint32_t raw_mv, int16_t offset_mv, int32_t gain_ppm)
{
    int64_t corrected = ((int64_t) raw_mv * (1000000 + (int64_t) gain_ppm)) / 1000000;
    corrected += offset_mv;

    if (corrected <= 0)
    {
        return 0;
    }
    if (corrected > UINT32_MAX)
    {
        return UINT32_MAX;
    }
    return (uint32_t) corrected;
}

uint8_t battery_percent_from_mv(uint32_t mv, const battery_curve_point_t * curve, size_t curve_size)
{
    if (curve == NULL || curve_size == 0)
    {
        curve      = kDefaultCurve;
        curve_size = sizeof(kDefaultCurve) / sizeof(kDefaultCurve[0]);
    }

    if (mv >= curve[0].mv)
    {
        return curve[0].percent_x2;
    }
    if (mv <= curve[curve_size - 1].mv)
    {
        return curve[curve_size - 1].percent_x2;
    }

    for (size_t i = 0; i + 1 < curve_size; i++)
    {
        const battery_curve_point_t * hi = &curve[i];
        const battery_curve_point_t * lo = &curve[i + 1];

        if (mv <= hi->mv && mv >= lo->mv && hi->mv > lo->mv)
        {
            int32_t span_mv  = (int32_t) hi->mv - (int32_t) lo->mv;
            int32_t span_pct = (int32_t) hi->percent_x2 - (int32_t) lo->percent_x2;
            int32_t offset   = (int32_t) mv - (int32_t) lo->mv;
            int32_t pct      = (int32_t) lo->percent_x2 + (offset * span_pct) / span_mv;

            if (pct < 0)
            {
                pct = 0;
            }
            if (pct > 200)
            {
                pct = 200;
            }
            return (uint8_t) pct;
        }
    }

    return 0;
}

battery_action_t battery_monitor_state_update(battery_monitor_state_t * state, uint32_t mv,
                                               const battery_monitor_config_t * cfg)
{
    if (state == NULL || cfg == NULL)
    {
        return BATTERY_ACTION_NONE;
    }

    uint8_t confirm = cfg->confirm_samples > 0 ? cfg->confirm_samples : 1;
    uint8_t protect = cfg->protect_samples > 0 ? cfg->protect_samples : 1;

    battery_level_t next = battery_level_next(state->level, mv, cfg);

    if (next != state->level)
    {
        if (next != state->candidate)
        {
            state->candidate       = next;
            state->candidate_count = 0;
        }
        if (state->candidate_count < UINT8_MAX)
        {
            state->candidate_count++;
        }
        if (state->candidate_count >= confirm)
        {
            state->level           = next;
            state->candidate       = next;
            state->candidate_count = 0;
        }
    }
    else
    {
        state->candidate       = state->level;
        state->candidate_count = 0;
    }

    /*
     * Protection is intentionally independent from the reporting level: a
     * battery that boots already below the threshold must be protected even
     * while the warning/critical ladder is still confirming.
     */
    if (mv <= cfg->protect_down_mv)
    {
        if (state->protect_count < UINT8_MAX)
        {
            state->protect_count++;
        }
    }
    else
    {
        state->protect_count = 0;
    }

    if (!state->latched && state->protect_count >= protect)
    {
        if (cfg->action != BATTERY_ACTION_NONE)
        {
            state->latched = true;
            return cfg->action;
        }
    }

    if (state->latched && mv >= cfg->protect_up_mv)
    {
        state->latched       = false;
        state->protect_count = 0;
    }

    return BATTERY_ACTION_NONE;
}
