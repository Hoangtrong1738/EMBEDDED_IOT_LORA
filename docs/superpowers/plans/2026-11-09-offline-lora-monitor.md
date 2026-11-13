# Offline LoRa Energy Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a complete local-only STM32-to-ESP32 LoRa telemetry monitor using deterministic simulated data.

**Architecture:** The STM32 project acquires a typed simulated sample through a single source module before packet serialization. The ESP32 project keeps radio I/O in `main.c`, moves pure validation/state/alarm logic into small testable modules, and passes a typed record to the existing LVGL UI.

**Tech Stack:** STM32Cube HAL, FreeRTOS, SX127x LoRa driver, ESP-IDF, PlatformIO, LVGL, PowerShell contract tests.

**Spec:** `docs/superpowers/specs/2026-11-09-offline-lora-monitor-design.md`

## Global Constraints

- Preserve the 25-byte telemetry payload and 4-byte ACK.
- RF values: 433 MHz, SF7, BW125, CR4/5, preamble 8, sync word `0x12`.
- No PZEM/DHT, network stack, MQTT, or packet-layout change.
- Node ID is 1 and data becomes stale after 30 seconds.

## Review Focus

- CRC failure, bad header, and wrong length must never generate an ACK or UI update; Task 3.
- A duplicate frame must receive ACK without inflating loss count or rendering again; Task 3.
- Sequence wraparound must not be treated as a large loss; Task 3.
- Power-to-energy accumulation must use the 2-second simulated interval and avoid overflow; Task 1.
- A stale gateway state must silence the buzzer even if the last packet alarmed; Task 4.

---

### Task 1: Create STM32 simulated telemetry source

**Files:**
- Create: `lora_stm32_pzem/software/inc/telemetry_source.h`
- Create: `lora_stm32_pzem/software/scr/telemetry_source.c`
- Modify: `lora_stm32_pzem/software/scr/node_lora.c`
- Modify: `lora_stm32_pzem/Makefile`
- Test: `lora_stm32_pzem/tests/test_telemetry_source_contract.ps1`

**Interfaces:**
- Produces `bool telemetry_source_read(telemetry_sample_t *out)`.
- `telemetry_sample_t` has scaled fields `volt_x10`, `current_mA`, `power_x10`, `energy_Wh`, `temp_x10`, `humidity_x10`, and `status`.

- [ ] **Step 1: Write the failing contract test**

Assert that the source has the typed API, bounded deterministic values, energy accumulation, and `node_lora.c` populates every telemetry field from the sample.

- [ ] **Step 2: Run the source contract test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\\tests\\test_telemetry_source_contract.ps1`

Expected: FAIL because the source module and its integration do not exist.

- [ ] **Step 3: Implement the source API and map it into the LoRa packet**

Use a deterministic periodic profile with normal and alarm states; store `energy_Wh` monotonically. Preserve CRC and ACK code.

- [ ] **Step 4: Run source test and STM32 build**

Run: `powershell -ExecutionPolicy Bypass -File .\\tests\\test_telemetry_source_contract.ps1; make -j`

Expected: contract passes and the firmware builds.

### Task 2: Introduce typed ESP32 packet and monitor state modules

**Files:**
- Create: `lora/components/gateway_core/include/gateway_core.h`
- Create: `lora/components/gateway_core/gateway_core.c`
- Create: `lora/components/gateway_core/CMakeLists.txt`
- Modify: `lora/src/CMakeLists.txt`
- Test: `lora/test/test_gateway_core_contract.ps1`

**Interfaces:**
- Produces `bool gateway_decode_packet(const uint8_t *bytes, size_t length, gateway_sample_t *out)`.
- Produces `gateway_rx_result_t gateway_track_sequence(gateway_sequence_state_t *state, uint16_t seq)`.

- [ ] **Step 1: Write failing core contract tests**

Assert explicit little-endian decoding, CRC validation, typed physical-unit conversion, first frame behavior, duplicate rejection, and `65535 -> 0` sequence wraparound.

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_core_contract.ps1`

Expected: FAIL because the gateway core module is absent.

- [ ] **Step 3: Implement gateway packet and sequence functions**

Make the decoder side-effect free. Treat only forward sequence gaps smaller than 100 as loss; expose duplicate separately.

- [ ] **Step 4: Run core contract test**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_core_contract.ps1`

Expected: PASS.

### Task 3: Wire the ESP32 radio loop to validated offline telemetry

**Files:**
- Modify: `lora/src/main.c`
- Modify: `lora/components/ui_gateway/include/ui_gateway.h`
- Modify: `lora/components/ui_gateway/ui_gateway.c`
- Test: `lora/test/test_gateway_integration_contract.ps1`

**Interfaces:**
- Consumes `gateway_decode_packet`, `gateway_track_sequence`, and the existing `ui_gw_update(const gw_ui_data_t *)`.
- Produces ACK for every valid frame, and a UI record only for new valid frames.

- [ ] **Step 1: Write the failing integration contract test**

Assert `main.c` rejects invalid frames before ACK, ACKs duplicates without UI updates, records RSSI/SNR, and maps all six telemetry values into `gw_ui_data_t`.

- [ ] **Step 2: Run integration test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_integration_contract.ps1`

Expected: FAIL because the radio loop does not use the core module or UI.

- [ ] **Step 3: Implement the radio-to-UI flow**

Use the existing SX127x configuration and ACK wire format. Update UI only after a new valid frame; keep UART diagnostics for valid, rejected, and duplicate traffic.

- [ ] **Step 4: Run integration test and ESP32 build**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_integration_contract.ps1; platformio run -e freenove_esp32_s3_wroom`

Expected: contract passes and PlatformIO reports success.

### Task 4: Add offline thresholds, freshness, and buzzer control

**Files:**
- Modify: `lora/components/gateway_core/include/gateway_core.h`
- Modify: `lora/components/gateway_core/gateway_core.c`
- Modify: `lora/src/main.c`
- Modify: `lora/components/ui_gateway/include/ui_gateway.h`
- Modify: `lora/components/ui_gateway/ui_gateway.c`
- Test: `lora/test/test_gateway_alarm_contract.ps1`

**Interfaces:**
- Produces `uint8_t gateway_alarm_eval(const gateway_sample_t *, const gateway_limits_t *)` and `bool gateway_is_fresh(int64_t now_us, int64_t last_rx_us)`.

- [ ] **Step 1: Write the failing alarm contract test**

Assert defaults 250 V, 10.0 A, 2000 W, 60 C, 90% RH; strict above-limit alarms; and stale data clears buzzer output.

- [ ] **Step 2: Run the alarm test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_alarm_contract.ps1`

Expected: FAIL because five-limit evaluation and fresh-data buzzer policy do not exist.

- [ ] **Step 3: Implement offline alarm policy**

Use a 30-second freshness cutoff and a 1-second on/off buzzer pattern. Extend the UI record with an alarm bitmask and show a specific alarm banner.

- [ ] **Step 4: Run all contract tests and both builds**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_core_contract.ps1; powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_integration_contract.ps1; powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_alarm_contract.ps1; platformio run -e freenove_esp32_s3_wroom; Set-Location ..\\lora_stm32_pzem; make -j`

Expected: all contract tests pass and both firmware builds complete successfully.
