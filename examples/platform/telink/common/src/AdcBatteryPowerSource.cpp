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

#include "AdcBatteryPowerSource.h"

#include <errno.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>

#if defined(CONFIG_SHELL)
#include <zephyr/shell/shell.h>
#endif

namespace chip::app {

namespace {

AdcBatteryPowerSource * sInstance = nullptr;

#if defined(CONFIG_SHELL)
int cmd_cluster(const struct shell * sh, size_t argc, char ** argv)
{
    (void) argc;
    (void) argv;

    if (sInstance == nullptr)
    {
        shell_error(sh, "battery power source is not registered");
        return -ENODEV;
    }

    auto & cluster = sInstance->BatteryPowerSourceCluster();
    auto voltage   = cluster.GetBatVoltage();
    auto percent   = cluster.GetBatPercentRemaining();

    if (voltage.IsNull())
    {
        shell_print(sh, "cluster: voltage=null");
    }
    else
    {
        shell_print(sh, "cluster: voltage=%u mV", voltage.Value());
    }

    if (percent.IsNull())
    {
        shell_print(sh, "cluster: percent=null");
    }
    else
    {
        shell_print(sh, "cluster: percent=%u/2 %%", percent.Value());
    }

    shell_print(sh, "cluster: level=%u", static_cast<unsigned>(cluster.GetBatChargeLevel()));
    return 0;
}

SHELL_CMD_REGISTER(bat_cluster, NULL, "Show PowerSource cluster attributes", cmd_cluster);
#endif

} // namespace

using namespace Clusters::PowerSource;

AdcBatteryPowerSource::AdcBatteryPowerSource() :
    BatteryPowerSource("Battery"_span, BatReplaceabilityEnum::kNotReplaceable, mTimerDelegate)
{
    sInstance = this;
}

AdcBatteryPowerSource::~AdcBatteryPowerSource()
{
    if (mMonitorStarted)
    {
        (void) battery_monitor_stop();
        mMonitorStarted = false;
    }

    if (sInstance == this)
    {
        sInstance = nullptr;
    }
}

CHIP_ERROR AdcBatteryPowerSource::Register(EndpointId endpoint, CodeDrivenDataModelProvider & provider,
                                           EndpointComposition composition)
{
    ReturnErrorOnFailure(BatteryPowerSource::Register(endpoint, provider, composition));

    battery_monitor_config_t config;

    battery_monitor_config_defaults(&config);
    config.action = BATTERY_ACTION_SAFE_MODE;

    int rc = battery_monitor_init(&config);
    if (rc != 0 && rc != -EALREADY)
    {
        ChipLogError(NotSpecified, "battery_monitor_init failed: %d", rc);
        return CHIP_ERROR_INTERNAL;
    }

    if (battery_monitor_is_latched())
    {
        ChipLogProgress(NotSpecified, "Battery: persisted low-battery latch, applying safe mode");
        battery_monitor_action_hook(BATTERY_ACTION_SAFE_MODE);
    }

    rc = battery_monitor_start(OnBatteryStatus, this);
    if (rc != 0)
    {
        ChipLogError(NotSpecified, "battery_monitor_start failed: %d", rc);
        return CHIP_ERROR_INTERNAL;
    }

    mMonitorStarted = true;
    return CHIP_NO_ERROR;
}

void AdcBatteryPowerSource::Unregister(CodeDrivenDataModelProvider & provider)
{
    if (mMonitorStarted)
    {
        (void) battery_monitor_stop();
        mMonitorStarted = false;
    }

    BatteryPowerSource::Unregister(provider);
}

void AdcBatteryPowerSource::OnBatteryStatus(const battery_status_t * status, void * user_data)
{
    auto * self = static_cast<AdcBatteryPowerSource *>(user_data);

    if (self == nullptr || status == nullptr || !status->valid)
    {
        return;
    }

    /*
     * The monitor runs in the Zephyr work queue; the data model must be
     * touched from the CHIP task. Only copy the scalar fields: the CHIP
     * lambda bridge is limited to CHIP_CONFIG_LAMBDA_EVENT_SIZE bytes.
     */
    TEMPORARY_RETURN_IGNORED DeviceLayer::SystemLayer().ScheduleLambda(
        [self, mv = status->voltage_mv, pct = status->percent_x2, level = static_cast<uint8_t>(status->level)] {
            self->Update(mv, pct, level);
        });
}

void AdcBatteryPowerSource::Update(uint32_t voltage_mv, uint8_t percent_x2, uint8_t level)
{
    auto & cluster = BatteryPowerSourceCluster();

    /* BatVoltage is changeOmitted: set it without generating a report. */
    cluster.SetBatVoltage(DataModel::Nullable<uint32_t>(voltage_mv));

    CHIP_ERROR err = cluster.SetBatPercentRemaining(DataModel::Nullable<uint8_t>(percent_x2));
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "SetBatPercentRemaining failed: %" CHIP_ERROR_FORMAT, err.Format());
    }

    err = cluster.SetBatChargeLevel(static_cast<BatChargeLevelEnum>(level));
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "SetBatChargeLevel failed: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

} // namespace chip::app

using namespace chip;

/*
 * Strong overrides of the weak hooks in BatteryMonitor.c. They run in the
 * Zephyr work queue, so the DeviceLayer calls are marshalled to the CHIP task.
 */
extern "C" void battery_monitor_action_hook(battery_action_t action)
{
    if (action != BATTERY_ACTION_SAFE_MODE)
    {
        return;
    }

    TEMPORARY_RETURN_IGNORED chip::DeviceLayer::SystemLayer().ScheduleLambda([] {
        ChipLogProgress(NotSpecified, "Battery: SAFE_MODE, disabling BLE advertising");
        (void) chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
    });
}

extern "C" void battery_monitor_recovery_hook(void)
{
    TEMPORARY_RETURN_IGNORED chip::DeviceLayer::SystemLayer().ScheduleLambda([] {
        ChipLogProgress(NotSpecified, "Battery: recovered, enabling BLE advertising");
        (void) chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(true);
    });
}
