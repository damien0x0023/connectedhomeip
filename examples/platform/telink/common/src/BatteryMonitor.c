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

#include "BatteryMonitor.h"
#include "BatteryMonitorLogic.h"

#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_SHELL)
#include <zephyr/shell/shell.h>
#endif

LOG_MODULE_REGISTER(battery_monitor, LOG_LEVEL_INF);

/* The board/app overlay must provide:
 *
 *   / { zephyr,user { io-channels = <&adc 0>; }; };
 *
 * with channel@0 using zephyr,input-positive = <DT_ADC_VBAT>.
 */
static const struct adc_dt_spec vbat_spec = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static K_MUTEX_DEFINE(state_lock);
static K_MUTEX_DEFINE(adc_lock);

static battery_monitor_config_t config;
static battery_monitor_state_t state;
static battery_status_t last_status;

static battery_status_cb_t status_cb;
static void * status_cb_user_data;

static struct k_work_delayable sample_work;
static bool started;
static bool initialized;

static bool sim_enabled;
static uint32_t sim_mv;

static int vbat_init(void)
{
    int rc;

    if (!adc_is_ready_dt(&vbat_spec))
    {
        LOG_ERR("VBAT ADC device is not ready");
        return -ENODEV;
    }

    /*
     * This is what puts the Telink SD-ADC into VBAT mode. It must run once
     * during initialization: adc_read_dt() does not configure the channel.
     */
    rc = adc_channel_setup_dt(&vbat_spec);
    if (rc != 0)
    {
        LOG_ERR("VBAT channel setup failed (%d)", rc);
        return rc;
    }

    return 0;
}

static int vbat_read_raw(uint32_t * mv)
{
    int16_t buf = 0;
    struct adc_sequence sequence = {
        .buffer      = &buf,
        .buffer_size = sizeof(buf),
    };
    int rc;

    rc = adc_sequence_init_dt(&vbat_spec, &sequence);
    if (rc != 0)
    {
        return rc;
    }

    k_mutex_lock(&adc_lock, K_FOREVER);
    rc = adc_read_dt(&vbat_spec, &sequence);
    k_mutex_unlock(&adc_lock);

    if (rc != 0)
    {
        return rc;
    }

    if (buf <= 0)
    {
        return -ERANGE;
    }

    /* The Telink ADC driver already converts the sample to millivolts. */
    *mv = (uint32_t) buf;
    return 0;
}

__weak void battery_monitor_action_hook(battery_action_t action)
{
    switch (action)
    {
    case BATTERY_ACTION_REBOOT:
#if defined(CONFIG_REBOOT)
        LOG_WRN("Battery protection: reboot");
        sys_reboot(SYS_REBOOT_WARM);
#else
        LOG_WRN("Battery protection: reboot requested, but CONFIG_REBOOT is disabled");
#endif
        break;
    case BATTERY_ACTION_SAFE_MODE:
        LOG_WRN("Battery protection: safe mode (not implemented yet)");
        break;
    case BATTERY_ACTION_SHUTDOWN:
        LOG_WRN("Battery protection: shutdown (not implemented yet)");
        break;
    case BATTERY_ACTION_NONE:
    default:
        break;
    }
}

static uint32_t next_period_ms(void)
{
    const battery_monitor_config_t * cfg = &config;
    uint32_t period                      = cfg->sample_period_ms;

    if (state.level != BATTERY_LEVEL_OK && cfg->low_sample_period_ms > 0)
    {
        period = cfg->low_sample_period_ms;
    }

    if (period < 100)
    {
        period = 100;
    }

    return period;
}

static void sample_and_dispatch(void)
{
    battery_monitor_config_t cfg;
    battery_monitor_state_t new_state;
    battery_status_t status;
    battery_action_t action;
    uint32_t raw_mv = 0;
    uint32_t mv;
    bool sim;
    uint32_t sim_value;
    int rc;

    k_mutex_lock(&state_lock, K_FOREVER);
    cfg       = config;
    sim       = sim_enabled;
    sim_value = sim_mv;
    k_mutex_unlock(&state_lock);

    if (sim)
    {
        raw_mv = sim_value;
        rc     = 0;
    }
    else
    {
        rc = vbat_read_raw(&raw_mv);
    }

    if (rc != 0)
    {
        LOG_WRN("VBAT sample failed (%d)", rc);
        k_work_reschedule(&sample_work, K_MSEC(next_period_ms()));
        return;
    }

    mv = battery_apply_calibration(raw_mv, cfg.cal_offset_mv, cfg.cal_gain_ppm);

    k_mutex_lock(&state_lock, K_FOREVER);
    action      = battery_monitor_state_update(&state, mv, &cfg);
    new_state   = state;
    status.voltage_mv     = mv;
    status.percent_x2     = battery_percent_from_mv(mv, cfg.curve, cfg.curve_size);
    status.level          = new_state.level;
    status.valid          = true;
    status.seq            = last_status.seq + 1;
    last_status           = status;
    k_mutex_unlock(&state_lock);

    if (status_cb != NULL)
    {
        status_cb(&status, status_cb_user_data);
    }

    if (action != BATTERY_ACTION_NONE)
    {
        battery_monitor_action_hook(action);
    }

    k_work_reschedule(&sample_work, K_MSEC(next_period_ms()));
}

static void sample_work_handler(struct k_work * work)
{
    ARG_UNUSED(work);
    sample_and_dispatch();
}

int battery_monitor_init(const battery_monitor_config_t * cfg)
{
    if (cfg == NULL)
    {
        return -EINVAL;
    }

    if (cfg->full_mv <= cfg->empty_mv)
    {
        return -EINVAL;
    }

    if (cfg->warn_down_mv <= cfg->crit_down_mv || cfg->warn_up_mv <= cfg->crit_up_mv)
    {
        return -EINVAL;
    }

    if (initialized)
    {
        return -EALREADY;
    }

    k_mutex_lock(&state_lock, K_FOREVER);
    config                 = *cfg;
    config.confirm_samples = (uint8_t) MAX(cfg->confirm_samples, 1);
    config.protect_samples = (uint8_t) MAX(cfg->protect_samples, 1);
    if (config.curve == NULL || config.curve_size == 0)
    {
        config.curve = battery_default_curve(&config.curve_size);
    }
    k_mutex_unlock(&state_lock);

    battery_monitor_state_reset(&state, BATTERY_LEVEL_OK);

    last_status.voltage_mv = 0;
    last_status.percent_x2 = 0;
    last_status.level      = BATTERY_LEVEL_OK;
    last_status.valid      = false;
    last_status.seq        = 0;

    k_work_init_delayable(&sample_work, sample_work_handler);

    int rc = vbat_init();
    if (rc != 0)
    {
        return rc;
    }

    initialized = true;
    return 0;
}

int battery_monitor_start(battery_status_cb_t cb, void * user_data)
{
    if (!initialized)
    {
        return -EINVAL;
    }

    k_mutex_lock(&state_lock, K_FOREVER);
    status_cb           = cb;
    status_cb_user_data = user_data;
    k_mutex_unlock(&state_lock);

    if (!started)
    {
        started = true;
        k_work_reschedule(&sample_work, K_MSEC(1));
    }

    return 0;
}

int battery_monitor_stop(void)
{
    if (!initialized)
    {
        return -EINVAL;
    }

    (void) k_work_cancel_delayable(&sample_work);
    started = false;
    return 0;
}

int battery_monitor_read_now(battery_status_t * out)
{
    uint32_t raw_mv = 0;
    uint32_t mv;
    bool sim;
    uint32_t sim_value;
    int rc;

    if (out == NULL || !initialized)
    {
        return -EINVAL;
    }

    k_mutex_lock(&state_lock, K_FOREVER);
    sim       = sim_enabled;
    sim_value = sim_mv;
    k_mutex_unlock(&state_lock);

    if (sim)
    {
        raw_mv = sim_value;
        rc     = 0;
    }
    else
    {
        rc = vbat_read_raw(&raw_mv);
    }

    if (rc != 0)
    {
        return rc;
    }

    k_mutex_lock(&state_lock, K_FOREVER);
    mv = battery_apply_calibration(raw_mv, config.cal_offset_mv, config.cal_gain_ppm);
    out->voltage_mv = mv;
    out->percent_x2 = battery_percent_from_mv(mv, config.curve, config.curve_size);
    out->level      = state.level;
    out->valid      = true;
    out->seq        = last_status.seq;
    k_mutex_unlock(&state_lock);

    return 0;
}

void battery_monitor_set_action(battery_action_t action)
{
    k_mutex_lock(&state_lock, K_FOREVER);
    config.action = action;
    k_mutex_unlock(&state_lock);
}

battery_action_t battery_monitor_get_action(void)
{
    battery_action_t action;

    k_mutex_lock(&state_lock, K_FOREVER);
    action = config.action;
    k_mutex_unlock(&state_lock);

    return action;
}

const battery_status_t * battery_monitor_get_status(void)
{
    return &last_status;
}

#if defined(CONFIG_SHELL)

static const char * level_name(battery_level_t level)
{
    switch (level)
    {
    case BATTERY_LEVEL_OK:
        return "OK";
    case BATTERY_LEVEL_WARNING:
        return "WARNING";
    case BATTERY_LEVEL_CRITICAL:
        return "CRITICAL";
    default:
        return "?";
    }
}

static int cmd_read(const struct shell * sh, size_t argc, char ** argv)
{
    battery_status_t status;
    int rc = battery_monitor_read_now(&status);

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (rc != 0)
    {
        shell_error(sh, "sample failed: %d", rc);
        return rc;
    }

    shell_print(sh, "VBAT %u mV, %u/2 %%, level %s", status.voltage_mv, status.percent_x2,
                level_name(status.level));
    return 0;
}

static int cmd_level(const struct shell * sh, size_t argc, char ** argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    k_mutex_lock(&state_lock, K_FOREVER);
    battery_monitor_state_t snapshot = state;
    battery_status_t status        = last_status;
    k_mutex_unlock(&state_lock);

    shell_print(sh, "level=%s candidate=%s count=%u protect=%u latched=%d last=%u mV seq=%u",
                level_name(snapshot.level), level_name(snapshot.candidate), snapshot.candidate_count,
                snapshot.protect_count, snapshot.latched ? 1 : 0, status.voltage_mv, status.seq);
    return 0;
}

static int cmd_sim(const struct shell * sh, size_t argc, char ** argv)
{
    if (argc != 2)
    {
        shell_error(sh, "usage: bat sim <mv|off>");
        return -EINVAL;
    }

    if (strcmp(argv[1], "off") == 0)
    {
        sim_enabled = false;
        shell_print(sh, "simulation disabled");
        return 0;
    }

    sim_mv      = (uint32_t) strtoul(argv[1], NULL, 0);
    sim_enabled = true;
    sample_and_dispatch();
    shell_print(sh, "simulation enabled: %u mV", sim_mv);
    return 0;
}

static int cmd_cal(const struct shell * sh, size_t argc, char ** argv)
{
    uint32_t actual_mv;
    uint32_t raw_mv = 0;
    int rc;

    if (argc != 2)
    {
        shell_error(sh, "usage: bat cal <actual_mv>");
        return -EINVAL;
    }

    actual_mv = (uint32_t) strtoul(argv[1], NULL, 0);
    rc        = vbat_read_raw(&raw_mv);
    if (rc != 0)
    {
        shell_error(sh, "sample failed: %d", rc);
        return rc;
    }

    k_mutex_lock(&state_lock, K_FOREVER);
    config.cal_offset_mv = (int16_t) ((int32_t) actual_mv - (int32_t) raw_mv);
    config.cal_gain_ppm  = 0;
    k_mutex_unlock(&state_lock);

    shell_print(sh, "raw=%u mV actual=%u mV offset=%d mV", raw_mv, actual_mv,
                (int32_t) actual_mv - (int32_t) raw_mv);
    return 0;
}

static int cmd_action(const struct shell * sh, size_t argc, char ** argv)
{
    if (argc != 2)
    {
        shell_error(sh, "usage: bat action <none|safe|shutdown|reboot>");
        return -EINVAL;
    }

    if (strcmp(argv[1], "none") == 0)
    {
        battery_monitor_set_action(BATTERY_ACTION_NONE);
    }
    else if (strcmp(argv[1], "safe") == 0)
    {
        battery_monitor_set_action(BATTERY_ACTION_SAFE_MODE);
    }
    else if (strcmp(argv[1], "shutdown") == 0)
    {
        battery_monitor_set_action(BATTERY_ACTION_SHUTDOWN);
    }
    else if (strcmp(argv[1], "reboot") == 0)
    {
        battery_monitor_set_action(BATTERY_ACTION_REBOOT);
    }
    else
    {
        shell_error(sh, "unknown action: %s", argv[1]);
        return -EINVAL;
    }

    shell_print(sh, "action set to %s", argv[1]);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(bat_commands,
                               SHELL_CMD_ARG(read, NULL, "Sample the battery now", cmd_read, 1, 0),
                               SHELL_CMD_ARG(level, NULL, "Show the state machine", cmd_level, 1, 0),
                               SHELL_CMD_ARG(sim, NULL, "Inject a voltage: sim <mv|off>", cmd_sim, 2, 0),
                               SHELL_CMD_ARG(cal, NULL, "Calibrate offset: cal <actual_mv>", cmd_cal, 2, 0),
                               SHELL_CMD_ARG(action, NULL, "Set protection action", cmd_action, 2, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(bat, &bat_commands, "Battery monitor", NULL);

#endif /* CONFIG_SHELL */
