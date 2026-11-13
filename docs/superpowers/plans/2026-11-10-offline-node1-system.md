# Offline Node 1 LoRa System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the offline STM32 Node 1 and ESP32-S3 gateway data, alarm, configuration, heartbeat, display, LED, and buzzer path while leaving power measurements at zero.

**Architecture:** Both devices use FreeRTOS event-driven tasks and exactly one radio-owning task. Pure state/protocol modules are host-tested; hardware tasks communicate through queues, task notifications, and overwrite-only latest-value records.

**Tech Stack:** STM32F103 HAL, FreeRTOS/CMSIS-RTOS v1, SX127x; ESP-IDF, FreeRTOS, LVGL 9, esp_lcd ILI9341 SPI-DMA; C11 protocol tests.

**Spec:** `docs/superpowers/specs/2026-11-10-offline-node1-system-design.md`

## Global Constraints

- Node ID is exactly 1.
- DHT11 and placeholder power periods are 1 second.
- Periodic telemetry is 10 seconds; alarm/fault transitions send immediately.
- MQTT, 4G, cloud storage, and multi-node state are excluded.
- Existing 25-byte telemetry, 4-byte ACK, 433 MHz, SF7, BW125, CR4/5, preamble 8, and sync word `0x12` remain compatible.
- DIO0 interrupt handlers notify tasks and never access SPI.

## Review Focus

- A malformed control-frame CRC must not change node limits; pin in Task 1 protocol tests.
- Sequence wrap and duplicate telemetry must not create false packet-loss growth; pin in Task 4 gateway tests.
- A persistent alarm must generate one immediate transition packet rather than one packet per sample; pin in Task 2 state tests.
- A DHT timeout/checksum failure must retain the previous valid reading while setting `ST_DHT_FAIL`; pin in Task 2 state tests.
- Missing optional LED/buzzer pins must not fail gateway startup; pin through disabled-pin branch and build in Task 6.

---

### Task 1: Shared control protocol

**Files:**
- Modify: `lora_stm32_pzem/software/inc/lora_data.h`
- Modify: `lora_stm32_pzem/software/scr/lora_data.c`
- Modify: `lora/src/gateway_protocol.h`
- Modify: `lora/src/gateway_protocol.c`
- Create: `lora_stm32_pzem/tests/test_node_protocol.c`
- Modify: `lora/test/test_gateway_protocol.c`

**Interfaces:**
- Produces: config, config ACK, heartbeat, and heartbeat ACK packed types and encode/decode helpers with CRC16-Modbus.

- [ ] Write failing tests for exact frame bytes, wrong CRC, wrong node, wrong length, and telemetry compatibility.
- [ ] Run the STM32 cross-compile and gateway host tests; confirm missing helpers fail.
- [ ] Implement the minimal packed control-frame helpers on both platforms.
- [ ] Run protocol tests and confirm they pass.

### Task 2: STM32 state and alarm transitions

**Files:**
- Create: `lora_stm32_pzem/software/inc/node_state.h`
- Create: `lora_stm32_pzem/software/scr/node_state.c`
- Create: `lora_stm32_pzem/tests/test_node_state.c`
- Modify: `lora_stm32_pzem/Makefile`

**Interfaces:**
- Produces: `node_state_init`, sensor/power/config update functions, atomic snapshot payload, and alarm-transition result.

- [ ] Write failing tests for defaults, zero power, valid/failed DHT updates, each threshold, transition-only notification, and config application.
- [ ] Compile the test and confirm missing state functions fail.
- [ ] Implement pure state logic without HAL/FreeRTOS dependencies.
- [ ] Run tests and confirm all cases pass.

### Task 3: STM32 FreeRTOS runtime and DIO0 event

**Files:**
- Rewrite: `lora_stm32_pzem/software/scr/node_lora.c`
- Modify: `lora_stm32_pzem/software/inc/node_lora.h`
- Modify: `lora_stm32_pzem/Core/Src/gpio.c`
- Modify: `lora_stm32_pzem/Core/Src/stm32f1xx_it.c`
- Modify: `lora_stm32_pzem/Core/Inc/stm32f1xx_it.h`
- Modify: `lora_stm32_pzem/Makefile`

**Interfaces:**
- Consumes: Task 1 control helpers and Task 2 state API.
- Produces: one-second DHT/power tasks, monitor task, ten-second periodic trigger, radio queue, DIO0 task notification, ACK/retry, config handling, heartbeat reply, status LEDs.

- [ ] Add compile-contract tests for ISR-to-task notification and one radio owner.
- [ ] Configure PB1 EXTI rising and its NVIC priority below the FreeRTOS syscall ceiling.
- [ ] Implement task/resource creation and periodic scheduling with `vTaskDelayUntil`.
- [ ] Implement radio event loop and downlink handlers.
- [ ] Build the complete STM32 firmware with warnings enabled.

### Task 4: Gateway protocol state and link supervision

**Files:**
- Create: `lora/src/gateway_state.h`
- Create: `lora/src/gateway_state.c`
- Create: `lora/test/test_gateway_state.c`
- Modify: `lora/src/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 protocol helpers and `gw_limits_t`.
- Produces: duplicate/loss accounting, last-seen time decisions, pending config sequence, heartbeat scheduling, and offline transition.

- [ ] Write failing tests for duplicate/wrap behavior, heartbeat success, 15-second offline boundary, and config ACK matching.
- [ ] Run host tests and confirm missing state API fails.
- [ ] Implement deterministic state functions using caller-supplied milliseconds.
- [ ] Run gateway host tests and confirm all pass.

### Task 5: Gateway event-driven radio runtime

**Files:**
- Rewrite: `lora/src/main.c`
- Modify: `lora/src/CMakeLists.txt`
- Modify: `lora/components/board_config/include/board_config.h`

**Interfaces:**
- Consumes: Tasks 1 and 4 plus `display_start`/`display_post_packet`.
- Produces: DIO0 ISR notification, sole radio task, processing queue, telemetry ACK, pending config transmission, heartbeat requests, and Node 1 online/offline updates.

- [ ] Add a contract test for configured DIO0 GPIO and task notification path.
- [ ] Initialize the gateway display through the existing display component.
- [ ] Implement GPIO ISR and the sole radio task on SPI3.
- [ ] Implement processing/supervisor task and queue-overwrite display updates.
- [ ] Build the ESP-IDF/PlatformIO project.

### Task 6: Offline LED/buzzer and UI state

**Files:**
- Create: `lora/components/gateway_alarm/CMakeLists.txt`
- Create: `lora/components/gateway_alarm/include/gateway_alarm.h`
- Create: `lora/components/gateway_alarm/gateway_alarm.c`
- Modify: `lora/components/board_config/include/board_config.h`
- Modify: `lora/components/display/include/display_task.h`
- Modify: `lora/components/display/display_task.c`
- Modify: `lora/components/ui_gateway/include/ui_gateway.h`
- Modify: `lora/components/ui_gateway/ui_gateway.c`
- Modify: `lora/src/CMakeLists.txt`

**Interfaces:**
- Produces: optional-pin-safe alarm initialization/update and explicit Node 1 online/offline UI updates.

- [ ] Implement disabled-by-default LED/buzzer GPIO configuration.
- [ ] Drive alarm output from alert mask and link state without blocking delays.
- [ ] Expose link-state updates through the display queue/task boundary.
- [ ] Disable performance refresh logging for production defaults.
- [ ] Build gateway and run UI wiring contract tests.

### Task 7: Full verification and handoff

**Files:**
- Modify: `lora_stm32_pzem/software/scr/Task.md` if present on disk.
- Modify: `lora/test/README`

**Interfaces:**
- Consumes: all prior tasks.
- Produces: reproducible build/test commands and hardware smoke-test sequence.

- [ ] Run every STM32 test/contract and `make all` from a clean dependency state.
- [ ] Run every gateway host/PowerShell test and the full PlatformIO/ESP-IDF build.
- [ ] Check packet sizes on both compilers and compare constants/settings.
- [ ] Document wiring assumptions, UART logs, expected heartbeat/telemetry timing, and how to enable actual alarm GPIOs.
