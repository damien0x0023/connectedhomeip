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

#include "BatteryReporter.h"

#include <errno.h>

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/power-source-server/power-source-server.h>
#include <app/util/MarkAttributeDirty.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>

#include "BatteryMonitor.h"

using namespace chip;
using namespace chip::app;

namespace {

using namespace chip::app::Clusters;

EndpointId sEndpoint = kInvalidEndpointId;

EndpointId FindBatteryEndpoint()
{
    for (EndpointId endpoint : EnabledEndpointsWithServerCluster(PowerSource::Id))
    {
        uint32_t featureMap = 0;

        if (PowerSource::Attributes::FeatureMap::Get(endpoint, &featureMap) ==
                Protocols::InteractionModel::Status::Success &&
            (featureMap & static_cast<uint32_t>(PowerSource::Feature::kBattery)) != 0)
        {
            return endpoint;
        }
    }

    return kInvalidEndpointId;
}

void OnBatteryStatus(const battery_status_t * status, void * userData)
{
    (void) userData;

    if (status == nullptr || !status->valid || sEndpoint == kInvalidEndpointId)
    {
        return;
    }

    const uint32_t voltage_mv = status->voltage_mv;
    const uint8_t percent_x2  = status->percent_x2;
    const uint8_t level       = static_cast<uint8_t>(status->level);

    /* The monitor runs in the Zephyr work queue; touch the data model from the CHIP task. */
    TEMPORARY_RETURN_IGNORED DeviceLayer::SystemLayer().ScheduleLambda([voltage_mv, percent_x2, level] {
        (void) PowerSource::Attributes::BatVoltage::Set(sEndpoint, voltage_mv, MarkAttributeDirty::kNo);
        (void) PowerSource::Attributes::BatPercentRemaining::Set(sEndpoint, percent_x2, MarkAttributeDirty::kIfChanged);
        (void) PowerSource::Attributes::BatChargeLevel::Set(
            sEndpoint, static_cast<PowerSource::BatChargeLevelEnum>(level), MarkAttributeDirty::kIfChanged);
    });
}

} // namespace

CHIP_ERROR BatteryReporter::Init()
{
    sEndpoint = FindBatteryEndpoint();
    if (sEndpoint == kInvalidEndpointId)
    {
        ChipLogError(NotSpecified, "BatteryReporter: no battery PowerSource endpoint");
        return CHIP_ERROR_NOT_FOUND;
    }

    EndpointId endpointList[] = { sEndpoint };
    CHIP_ERROR err = Clusters::PowerSourceServer::Instance().SetEndpointList(sEndpoint, Span<EndpointId>(endpointList, 1));
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "BatteryReporter: SetEndpointList failed: %" CHIP_ERROR_FORMAT, err.Format());
    }

    battery_monitor_config_t config;

    battery_monitor_config_defaults(&config);
    config.action = BATTERY_ACTION_SAFE_MODE;

    int rc = battery_monitor_init(&config);
    if (rc != 0 && rc != -EALREADY)
    {
        ChipLogError(NotSpecified, "BatteryReporter: battery_monitor_init failed: %d", rc);
        return CHIP_ERROR_INTERNAL;
    }

    if (battery_monitor_is_latched())
    {
        battery_monitor_action_hook(BATTERY_ACTION_SAFE_MODE);
    }

    rc = battery_monitor_start(OnBatteryStatus, nullptr);
    if (rc != 0)
    {
        ChipLogError(NotSpecified, "BatteryReporter: battery_monitor_start failed: %d", rc);
        return CHIP_ERROR_INTERNAL;
    }

    ChipLogProgress(NotSpecified, "BatteryReporter: reporting on endpoint %u", sEndpoint);
    return CHIP_NO_ERROR;
}
