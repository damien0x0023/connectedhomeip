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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "BatteryMonitor.h"

LOG_MODULE_REGISTER(battery_monitor_sample, LOG_LEVEL_INF);

static void on_status(const battery_status_t * status, void * user_data)
{
    ARG_UNUSED(user_data);

    LOG_INF("VBAT %u mV, %u/2 %%, level %d, seq %u", status->voltage_mv, status->percent_x2,
            status->level, status->seq);
}

int main(void)
{
    battery_monitor_config_t config;
    int rc;

    battery_monitor_config_defaults(&config);

    /* Keep the demo readable on the console. */
    config.sample_period_ms     = 1000;
    config.low_sample_period_ms = 1000;

    rc = battery_monitor_init(&config);
    if (rc != 0)
    {
        LOG_ERR("battery_monitor_init failed (%d)", rc);
        return 0;
    }

    rc = battery_monitor_start(on_status, NULL);
    if (rc != 0)
    {
        LOG_ERR("battery_monitor_start failed (%d)", rc);
        return 0;
    }

    LOG_INF("battery monitor sample started");
    return 0;
}
