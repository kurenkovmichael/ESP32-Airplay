# ESP32-WROVER + ADAU1701

The `esp32wrover-adau1701` target supports an ESP32-WROVER-IE N16R8 connected to
an ADAU1701 DSP. The DSP self-boots from EEPROM and acts as the I2S clock master;
the ESP32 sends audio as a clock slave.

## Clock and data format

The ADAU1701 EEPROM program must provide continuous clocks with this topology:

| Signal | ADAU1701 | ESP32 | Configuration |
| --- | --- | --- | --- |
| BCLK | MP11 output | GPIO14 input | 64 × sample rate |
| LRCLK | MP10 output | GPIO27 input | 44.1 kHz |
| SDATA | input | GPIO26 output | 16-bit PCM in 32-bit slots |

The board defaults enable `CONFIG_I2S_OUTPUT_ROLE_SLAVE` and
`CONFIG_I2S_32BIT_OUTPUT`. The external clock fixes the output rate at
44.1 kHz; runtime requests to change it are ignored with a warning.

## Build and flash

```bash
pio run -e esp32wrover-adau1701 -t upload
pio run -e esp32wrover-adau1701 -t uploadfs
```

The target uses the full 16 MB flash and enables the WROVER module's 8 MB PSRAM.

## Volume control

Software attenuation on the ESP32 is enabled by default. No I2C connection to
the ADAU1701 is required for this mode.

To control a linked stereo gain block through ADAU1701 safeload, enable hardware
volume and set the parameter address exported by SigmaStudio in
`config/sdkconfig.defaults.esp32wrover-adau1701`:

```text
CONFIG_DAC_CONTROLS_VOLUME=y
CONFIG_ADAU1701_VOLUME_PARAM_ADDR=<decimal address from SigmaStudio>
```

The parameter address is required whenever ADAU1701 hardware volume control is
enabled. Valid addresses are 0 through 1023. The gain block must use standard
5.23 coefficients, where unity is `0x00800000`.

The ADAU1701 has the fixed 8-bit I2C address `0x34`. On this board, its physical
SDA line connects to ESP32 GPIO22 and SCL connects to GPIO21.
