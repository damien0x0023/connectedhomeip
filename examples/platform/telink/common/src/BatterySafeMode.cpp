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

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>

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

    TEMPORARY_RETURN_IGNORED DeviceLayer::SystemLayer().ScheduleLambda([] {
        ChipLogProgress(NotSpecified, "Battery: SAFE_MODE, disabling BLE advertising");
        (void) DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
    });
}

extern "C" void battery_monitor_recovery_hook(void)
{
    TEMPORARY_RETURN_IGNORED DeviceLayer::SystemLayer().ScheduleLambda([] {
        ChipLogProgress(NotSpecified, "Battery: recovered, enabling BLE advertising");
        (void) DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(true);
    });
}
