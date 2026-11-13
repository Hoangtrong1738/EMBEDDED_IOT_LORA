# Gateway Full UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate the supplied touchscreen dashboard and persisted alert settings into the offline ESP32-S3 LoRa gateway.

**Architecture:** Keep `gateway_protocol` as the packet-validation boundary. `main.c` will calculate alarm bits from `gw_settings`, then post a normalized `gw_ui_data_t` to `display_task`; the supplied UI owns LVGL state, touch events and settings navigation.

**Tech Stack:** ESP-IDF, LVGL 9, esp_lvgl_port, ILI9341, XPT2046, NVS, FreeRTOS.

**Spec:** `docs/superpowers/specs/2026-11-09-gateway-full-ui-design.md`

## Global Constraints

- Offline only: 4G and MQTT remain disabled.
- LoRa packet is 25 bytes; ACK is 4 bytes.
- Use explicit, board-configured touch pins; do not guess a buzzer GPIO.

## Review Focus

- Invalid packet never updates the display or gets ACKed.
- A duplicate packet is ACKed but does not repaint counters or telemetry.
- A packet stale for 30 seconds renders the LoRa-offline state.
- Settings are clamped to their declared min/max before being stored in NVS.
- Touch remains disabled cleanly until CS/IRQ pins are confirmed.

---

### Task 1: Import shared UI and settings components

**Files:**
- Create: `lora/components/gw_common/include/gw_features.h`
- Create: `lora/components/gw_common/include/gw_types.h`
- Create: `lora/components/gw_settings/*`
- Create: `lora/components/gw_ui/*`
- Modify: `lora/CMakeLists.txt`, `lora/src/CMakeLists.txt`

- [ ] Write a failing component-resolution/build check.
- [ ] Copy supplied UI/settings sources and CMake manifests; disable touch until pin values are configured.
- [ ] Run PlatformIO build and confirm CMake resolves all new component dependencies.

### Task 2: Connect validated LoRa telemetry to the display task

**Files:**
- Modify: `lora/src/main.c`
- Modify: `lora/src/gateway_protocol.*`
- Test: `lora/test/test_gateway_protocol.c`

- [ ] Add a failing test for converting a decoded packet and configured limits into the five alarm bits.
- [ ] Read persisted limits in app startup; replace direct UI updates with `display_post_packet()`.
- [ ] Run the protocol contract test and PlatformIO build.

### Task 3: Verify the complete dashboard lifecycle

**Files:**
- Test: `lora/test/test_gateway_telemetry_contract.ps1`

- [ ] Add coverage for queue posting, duplicate ACK behavior, and settings initialization.
- [ ] Run both gateway tests and an ESP32 build.
- [ ] Flash only after the user connects the board and confirms the target port.
