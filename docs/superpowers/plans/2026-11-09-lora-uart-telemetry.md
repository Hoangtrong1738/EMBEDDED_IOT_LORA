# LoRa UART Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Print validated STM32 telemetry received over LoRa on the ESP32 UART console.

**Architecture:** `main.c` keeps validation and ACK ownership. Small little-endian helpers decode the packed frame directly from its byte array, and one formatter logs the decoded values after validation.

**Tech Stack:** ESP-IDF, C, ESP_LOG, PlatformIO.

**Spec:** `docs/superpowers/specs/2026-11-09-lora-uart-telemetry-design.md`

## Global Constraints

- Preserve the 25-byte `lora_data_pkt_t` LoRa payload and 4-byte ACK.
- Decode every multibyte telemetry field as little-endian.
- Do not print telemetry for invalid frames.

## Review Focus

- 32-bit current, power, and energy must not be truncated; test owns Task 1.
- Signed temperature must retain negative values; test owns Task 1.
- Invalid CRC/header/length must not reach the UART formatter; existing validator path owns Task 1.
- ACK is still sent after a valid telemetry line; integration build owns Task 1.

---

### Task 1: Decode and print validated telemetry

**Files:**
- Modify: `lora/src/main.c`
- Create: `lora/test/test_gateway_telemetry_contract.ps1`

**Interfaces:**
- Consumes: a validated 25-byte telemetry byte array.
- Produces: one UART log line beginning `Telemetry node=`.

- [ ] **Step 1: Write the failing contract test**

Assert that `main.c` has explicit little-endian 16/32-bit decoding and an `ESP_LOGI` telemetry line containing all requested fields.

- [ ] **Step 2: Run the contract test to verify it fails**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_telemetry_contract.ps1`

Expected: FAIL because the telemetry formatter does not exist.

- [ ] **Step 3: Implement explicit decoding and UART formatter in `lora/src/main.c`**

Use byte offsets matching `lora_data_pkt_t`; call the formatter only after `telemetry_is_valid()` and before `lora_send_packet()`.

- [ ] **Step 4: Run the contract test and PlatformIO build**

Run: `powershell -ExecutionPolicy Bypass -File .\\test\\test_gateway_telemetry_contract.ps1; platformio run -e freenove_esp32_s3_wroom`

Expected: contract test passes and PlatformIO reports success.
