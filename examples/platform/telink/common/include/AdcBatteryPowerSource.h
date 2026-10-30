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

#include <device/types/power-source/BatteryPowerSource.h>
#include <platform/DefaultTimerDelegate.h>

#include "BatteryMonitor.h"

namespace chip::app {

/**
 * Battery power source backed by the Telink VBAT ADC.
 *
 * It replaces the simulated power-source device type in the all-devices app
 * when CONFIG_TELINK_BATTERY_MONITOR is enabled.
 */
class AdcBatteryPowerSource : public BatteryPowerSource
{
public:
    AdcBatteryPowerSource();
    ~AdcBatteryPowerSource() override;

    CHIP_ERROR Register(EndpointId endpoint, CodeDrivenDataModelProvider & provider,
                        EndpointComposition composition = {}) override;
    void Unregister(CodeDrivenDataModelProvider & provider) override;

private:
    static void OnBatteryStatus(const battery_status_t * status, void * user_data);
    void Update(uint32_t voltage_mv, uint8_t percent_x2, uint8_t level);

    DefaultTimerDelegate mTimerDelegate;
    bool mMonitorStarted = false;
};

} // namespace chip::app
