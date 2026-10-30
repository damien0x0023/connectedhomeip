#
#    Copyright (c) 2026 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

cmake_minimum_required(VERSION 3.20)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(telink_battery_monitor_logic)

set(BM_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/../../common)

target_include_directories(app PRIVATE ${BM_COMMON}/include)

target_sources(app PRIVATE
               src/main.c
               ${BM_COMMON}/src/BatteryMonitorLogic.c)
