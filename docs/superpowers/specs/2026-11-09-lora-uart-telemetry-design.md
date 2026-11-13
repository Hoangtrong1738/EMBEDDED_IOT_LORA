# LoRa UART Telemetry Design

## Intent

Show the simulated STM32 telemetry on the ESP32 serial console after a valid LoRa frame is received.

## Goals

- Preserve the existing 25-byte packed `lora_data_pkt_t` wire layout.
- Print node ID, sequence, voltage, current, power, energy, temperature, humidity, status, RSSI, and SNR on ESP32 UART.
- Keep ACK behavior unchanged.

## Non-goals

- No MQTT, cloud API, or packet-format change.
- No change to STM32 simulated telemetry values.

## Approach

Decode multibyte fields explicitly as little-endian in `lora/src/main.c` after `telemetry_is_valid()` succeeds, then emit one `ESP_LOGI` telemetry line before transmitting the ACK. This is selected over casting the byte buffer to a C struct so the ESP32 receiver is independent of alignment and compiler packing.

## Constraints

- RF settings and the ACK packet remain unchanged.
- UART output is diagnostic console output only.
