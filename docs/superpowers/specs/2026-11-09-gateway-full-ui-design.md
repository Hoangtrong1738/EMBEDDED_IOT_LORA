# Gateway Full UI Design

## Intent

Replace the minimal ESP32-S3 telemetry screen with the supplied full offline UI: main dashboard, LoRa stale state, touch settings, persisted limits, and alarm banners.

## Goals

- Reuse the supplied `gw_ui` and `gw_settings` implementation and 320x240 ILI9341 layout.
- Preserve the existing SX127x frame validation, ACK, duplicate handling, and packet-loss counting.
- Route telemetry to the display queue and calculate the five alarm bits from persisted limits.
- Support XPT2046 touch with hardware pins configured in one board header.

## Non-goals

- Do not add 4G, MQTT, or a physical buzzer GPIO without a confirmed pin.
- Do not change the STM32 frame format or LoRa parameters.

## Approach

Copy the supplied UI/settings components into the gateway, add a small `gw_common` header for shared alarm bits and offline feature selection, and make `main.c` own telemetry-to-display conversion. The display task remains the sole owner of settings navigation and flash writes.

## Hardware constraint

The supplied UI uses XPT2046 touch but its pin mapping was not supplied in the target workspace. The integration must add explicit placeholder/disabled touch configuration until the user confirms touch CS and IRQ pins.
