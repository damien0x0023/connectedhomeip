# TL5218X Lighting App Build Guide

## 1. 2MB Flash (Matter only, NO OTA/MCUBoot, NO LZMA)

**Kconfig:** boards/tl5218x.conf (default)

-   CONFIG_CHIP_OTA_REQUESTOR=n
-   CONFIG_CHIP_DFU_OVER_BT_SMP=n, CONFIG_CHIP_DFU_OVER_BT_SMP_BUILD=n,
-   CONFIG_LZMA=n, CONFIG_COMPRESS_LZMA=n
-   CONFIG_DUAL_MODE=0

**Note**: This configuration disables OTA, does not use MCUBoot, and cannot be
upgraded

**DTS Overlay:** src/platform/telink/tl5218x_2m_flash.overlay (default)

-   slot0: 0x15000, size 0xe7000 (924KB)
-   slot1: 0xfc000, size 0xe7000 (924KB)

**Build:**

```bash
west build -p -b tl5218x -d build_tl5218x_default -- \
> build_tl5218x_default.log
# Kconfig: default (boards/tl5218x.conf)
# DTS Overlay: default (src/platform/telink/tl5218x_2m_flash.overlay)
```

**Output:**

```bash
build_tl5218x_default/zephyr/zephyr.bin          -> flash this
```

## 2. 2MB Flash (Matter only, enable BT DFU, LZMA compressed, SW - 1 and 2)

**IMPORTANT**: LZMA compression is REQUIRED for 2MB flash with OTA.

**Kconfig:** boards/tl5218x_2m_flash_ota_lzma.conf (explicitly specified)

-   CONFIG_CHIP_OTA_REQUESTOR=y
-   CONFIG_CHIP_DFU_OVER_BT_SMP=y, CONFIG_CHIP_DFU_OVER_BT_SMP_BUILD=y,
-   CONFIG_LZMA=y, CONFIG_COMPRESS_LZMA=y
-   CONFIG_DUAL_MODE=0

**DTS Overlay:** src/platform/telink/tl5218x_2m_flash_lzma.overlay (default)

-   slot0: 0x15000, size 0x120000 (1152KB)
-   slot1: 0x135000, size 0xAB000 (684KB)

**Build Software Version 1:**

```bash
west build -p -b tl5218x -d build_tl5218x_2m_flash_lzma_v1 -- \
-DCONF_FILE="prj.conf boards/tl5218x_2m_flash_ota_lzma.conf" \
-DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=1 > build_tl5218x_2m_flash_lzma_v1.log
# Kconfig: explicitly specified (-DCONF_FILE="prj.conf boards/tl5218x_2m_flash_ota_lzma.conf")
# DTS Overlay: default (src/platform/telink/tl5218x_2m_flash_lzma.overlay)
```

**Build Software Version 2:**

```bash
west build -p -b tl5218x -d build_tl5218x_2m_flash_lzma_v2 -- \
-DCONF_FILE="prj.conf boards/tl5218x_2m_flash_ota_lzma.conf" \
-DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=2 > build_tl5218x_2m_flash_lzma_v2.log
# Kconfig: explicitly specified (-DCONF_FILE="prj.conf boards/tl5218x_2m_flash_ota_lzma.conf")
# DTS Overlay: default (src/platform/telink/tl5218x_2m_flash_lzma.overlay)
```

**Output:**

```bash
build_tl5218x_2m_flash_lzma_v1/zephyr/merged.bin          -> flash this
build_tl5218x_2m_flash_lzma_v2/zephyr/merged_dfu.lzma.bin -> DFU over BLE SMP
build_tl5218x_2m_flash_lzma_v2/zephyr/matter.ota          -> OTA upgrade
```

## 3. 4MB Flash (Matter + Zigbee dual mode, no LZMA)

**Prerequisite:** copy Zigbee firmware

```bash
cp <zigbee_fw>.bin ${ZEPHYR_BASE}/TL521X_FW/ZB/Zigbee-SampleDemo.bin
e.g.
cp ${ZEPHYR_BASE}/TL521X_FW/ZB/zigbee_bleAdv_sampleLight_tl521x.bin ${ZEPHYR_BASE}/TL521X_FW/ZB/Zigbee-SampleDemo.bin
```

**Kconfig:** boards/tl5218x_4m_flash_dual_mode_ota.conf (explicitly specified)

-   CONFIG_CHIP_OTA_REQUESTOR=y
-   CONFIG_CHIP_DFU_OVER_BT_SMP=y, CONFIG_CHIP_DFU_OVER_BT_SMP_BUILD=y,
-   CONFIG_LZMA=n, CONFIG_COMPRESS_LZMA=n
-   CONFIG_DUAL_MODE=2 (auto-switch mode)

**DTS Overlay:** src/platform/telink/tl5218x_4m_flash.overlay (default)

-   slot0: 0x16000, size 0x1E5000 (1940KB)
-   slot1: 0x1FB000, size 0x1E5000 (1940KB)

**Build:**

```bash
west build -p -b tl5218x -d build_tl5218x_4m_dual_mode -- \
-DFLASH_SIZE=4m \
-DCONF_FILE="prj.conf boards/tl5218x_4m_flash_dual_mode_ota.conf" > build_tl5218x_4m_dual_mode.log
# Kconfig: explicitly specified (-DCONF_FILE="prj.conf boards/tl5218x_4m_flash_dual_mode_ota.conf")
# DTS Overlay: default (src/platform/telink/tl5218x_4m_flash.overlay)
```

**Output:**

```bash
build_tl5218x_4m_dual_mode/zephyr/merged.bin      -> flash this (sboot+mcuboot+matter+zb)
build_tl5218x_4m_dual_mode/zephyr/merged_dfu.bin  -> DFU over BLE SMP
build_tl5218x_4m_dual_mode/zephyr/matter.ota      -> OTA upgrade
```

## 4. WARNING: 2MB Flash with OTA but NO LZMA (NOT RECOMMENDED)

**This configuration will show a warning during build and may fail at link
time.**

**Kconfig:** boards/tl5218x_ota.conf (explicitly specified)

-   CONFIG_CHIP_OTA_REQUESTOR=y
-   CONFIG_CHIP_DFU_OVER_BT_SMP=y, CONFIG_CHIP_DFU_OVER_BT_SMP_BUILD=y,
-   CONFIG_LZMA=n, CONFIG_COMPRESS_LZMA=n
-   CONFIG_DUAL_MODE=0

**WARNING**: Building this configuration will show a warning:

> "tl5218x with 2MB flash and OTA requires LZMA compression. Please use a config
> with 'lzma' suffix and set CONFIG_COMPRESS_LZMA=y. Continuing compilation
> anyway..."

**Note**: Build may fail at link time if firmware size exceeds 924KB (0xe7000)
partition limit.

**Solution**: Use the LZMA-enabled configuration (target #2) instead.

## 5. TL5218C-EVK40D (40-pin) vs TL5218X-EVK64D (64-pin)

The shared `tl5218x` board DTS keeps the EVK64D default:

-   UART0 TX = **PB5**
-   UART0 RX = **PB0**

The TL5218C-EVK40D uses a 40-pin package. On this board:

-   **PB0 is a JTAG/strap pin and is not brought out on J25/J26.**
-   UART_RX is routed to **PB6**, available on **J26 pins 29/30**.
-   UART_TX is still **PB5**, available on **J26 pins 27/28**.
-   `UART0_RTX` (function 24) can be selected on PB6 through the PB6 function
    select register.

To keep both boards working, do **not** change the shared board DTS. Use the
EVK40D overlay instead:

[`boards/tl5218x_for_TL5218C-EVK40D.overlay`](tl5218x_for_TL5218C-EVK40D.overlay)

The overlay:

1.  Adds a `uart0_rx_pb6_default` pinctrl state with
    `TL521X_PINMUX_SET(TL521X_PORT_B, TL521X_PIN_6, TL521X_FUNC_UART0_RTX_IO)`.
2.  Overrides `&uart0` to use `PB5` TX + `PB6` RX.

### 5.1 Build for TL5218X-EVK64D (default, RX = PB0)

No overlay is needed:

```bash
west build -p -b tl5218x -d build_tl5218x_evk64d . -- \
  -DFLASH_SIZE=4m
```

### 5.2 Build for TL5218C-EVK40D (RX = PB6)

Pass the overlay explicitly:

```bash
west build -p -b tl5218x -d build_tl5218x_evk40d . -- \
  -DFLASH_SIZE=4m \
  -DDTC_OVERLAY_FILE="boards/tl5218x_for_TL5218C-EVK40D.overlay"
```

The build log should show:

```text
Telink MCUboot board overlay: .../boards/tl5218x_for_TL5218C-EVK40D.overlay
```

### 5.3 Build for TL5218C-EVK40D with MCUboot + secure programming

The same overlay can be combined with MCUboot and secure programming:

```bash
west build -p -b tl5218x -d build_tl5218x_evk40d_mcuboot . -- \
  -DFLASH_SIZE=4m \
  -DDTC_OVERLAY_FILE="boards/tl5218x_for_TL5218C-EVK40D.overlay" \
  -DCONFIG_BOOTLOADER_MCUBOOT=y \
  -DCONFIG_CHIP_FACTORY_DATA=y \
  -DCONFIG_CHIP_CERTIFICATION_DECLARATION_STORAGE=y \
  -DCONFIG_CHIP_FACTORY_DATA_BUILD=n \
  -DCONFIG_CHIP_FACTORY_DATA_MERGE_WITH_FIRMWARE=n \
  -DCONFIG_SECURE_PROGRAMMING=y
```

Flash `zephyr/merged.bin` (MCUboot + signed app), not `zephyr.bin`:

```bash
windows-download-tool download <build>/zephyr/merged.bin --addr 0
```

### 5.4 Why the overlay also reaches MCUboot

The MCUboot image is built as a separate `west build` sub-project by
`config/telink/chip-module/CMakeLists.txt`. It does not automatically inherit
the application's `DTC_OVERLAY_FILE`.

To keep the MCUboot chip-ID trigger pin consistent:

-   `examples/platform/telink/common.cmake` saves the user-supplied overlay
    into `TELINK_APP_DTC_OVERLAY_FILE`.
-   `config/telink/chip-module/CMakeLists.txt` forwards it to the MCUboot
    sub-build via `MCUBOOT_DTC_OVERLAYS`, together with the boot/flash
    overlays.

So with the EVK40D overlay, both the application and MCUboot use PB6 as
UART0 RX. Without it, both use the EVK64D default PB0.

### 5.5 Verify the selected RX pin

After configuring, check both generated DTS files:

```bash
grep -n "uart0_rx_pb6_default" \
  <build>/zephyr/zephyr.dts \
  <build>/modules/chip-module/build_mcuboot/zephyr/zephyr.dts
```

Both should show:

```text
pinctrl-0 = < &uart0_tx_pb5_default &uart0_rx_pb6_default >;
```

### 5.6 MCUboot chip-ID trigger on EVK40D

The MCUboot startup hook (`soc/telink/tlx/telink_tl521x/mcu_boot_startup.c`)
reads the console UART RX pin from the `uart0` pinctrl. With the EVK40D
overlay, that pin is PB6.

To print the chip ID:

1.  Short **J26 pin 29/30 (PB6)** to **GND**.
2.  Reset the board.
3.  Capture the UART on PB5 (115200 8N1).

Expected output (A0 sample, eFuse 213 erased):

```text
AA 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 55
CHIP_ID:aa12...0055
```

The 21-byte format is `AA 12` + 8-byte IEEE address + 8 zero bytes + CRC16 +
`55`. Release PB6 from GND after the test; with PB6 high the chip-ID print is
skipped on the next boot.

> Note: on the A0 sample both eFuse 197 (chip_id) and eFuse 213 (MAC) are
> erased (all `0x00`), so the 16-byte key used for DAC decryption is
> `16 × 0x00`. Do not use this configuration for production.
