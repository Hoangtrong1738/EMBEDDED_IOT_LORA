# Offline LoRa Energy Monitor Design

## Intent

Complete the offline LoRa monitoring path in `D:\\EMBEDDED_IOT_LORA`: an STM32F103 node emits simulated electrical and environmental telemetry, and the existing ESP32-S3 gateway accepts, acknowledges, displays, and alarms on valid packets without any cloud service.

## Goals

- Keep the 25-byte telemetry payload and 4-byte ACK frame already used by the STM32 and ESP32 projects.
- Replace hard-coded packet fields on STM32 with one simulated telemetry source API.
- Keep exactly one later replacement point for PZEM/DHT data acquisition.
- Make the ESP32 gateway decode valid packets, reject invalid/duplicate frames, send ACKs, update its LVGL UI, and emit UART diagnostics.
- Evaluate offline configurable limits for voltage, current, power, temperature, and humidity; drive the buzzer when a fresh packet is in alarm.

## Non-goals

- No PZEM, DHT11, 4G, MQTT, Node-RED, remote configuration, or cloud storage.
- No packet-layout change.

## Architecture

The STM32 owns a `telemetry_source_read()` function that produces deterministic, bounded simulated measurements and increments energy from the simulated power. `node_lora` maps this typed sample into the existing packed wire packet and retains retransmission/ACK behavior.

The ESP32 gateway retains the existing `lora` project. `main.c` owns radio receive, validation, duplicate detection, ACK generation, and conversion to a typed gateway sample. A small offline monitor module owns limits, alarm evaluation, loss counting, and freshness. `ui_gateway` receives the typed display record and remains the sole owner of LVGL updates.

## Constraints

- Radio settings remain 433 MHz, SF7, BW125, CR4/5, preamble 8, and sync word `0x12`.
- Node ID is 1 for this phase.
- The simulated source updates every 2 seconds and must exercise both normal and over-limit values during a repeatable cycle.
- ACK is issued only for a structurally valid, CRC-valid telemetry packet; a duplicate valid packet is ACKed but not rendered twice.
- Gateway declares data stale after 30 seconds and silences the buzzer.
- Default limits: 250 V, 10.0 A, 2000 W, 60 C, 90% RH.
