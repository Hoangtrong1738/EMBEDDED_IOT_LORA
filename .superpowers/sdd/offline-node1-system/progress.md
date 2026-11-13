# SDD ledger — plan: docs/superpowers/plans/2026-11-10-offline-node1-system.md

Setup ruling: workspace has no Git metadata, so worktree/commit/range scripts cannot operate; implementation is performed in place and verified by builds/tests. Cost if wrong: no automatic commit-level rollback boundary.
Pre-flight: Task 1 protocol types feed Tasks 3–5; one-byte packed frames and little-endian fields are authoritative from the spec.
Pre-flight: Task 2 state snapshot feeds Task 3 telemetry; wire units remain V, mA, mW, Wh, C, and %RH.
Pre-flight: Task 4 gateway state feeds Tasks 5–6; caller-supplied monotonic milliseconds avoids coupling pure tests to ESP timers.
Ruling: optional alarm GPIOs default to -1 because product LED/buzzer pins were not supplied; logic remains active and startup must succeed. Cost if wrong: no physical alarm until pins are configured.
Ruling: ESP32-S3 DIO0 defaults to GPIO16 because the PCB route was not supplied; a 20 ms register-poll fallback remains active. Cost if wrong: interrupts are unavailable until the macro is corrected, but reception still functions by polling.
Ruling: STM32 buzzer support is compiled behind `NODE_BUZZER_ENABLED=0`; PA8 is only a suggested output. Cost if wrong: enabling without confirming the PCB may drive an unrelated signal.
Task 1 complete: control protocol/CRC helpers and compatibility tests compile on both targets.
Task 2 complete: pure Node 1 state, retained DHT sample, zero-power placeholder, thresholds, and transition-only alarm logic implemented.
Task 3 complete: STM32 one-second DHT/power tasks, monitor task, sole radio task, PB1 EXTI notification, ACK/retry, config, heartbeat, LEDs, and configurable buzzer path build successfully.
Task 4 complete: gateway duplicate/loss, heartbeat, config ACK, and 15-second offline state implemented.
Task 5 complete: sole ESP32 radio owner, DIO0 notification plus polling fallback, queues, ACK, config, heartbeat, and Node 1 process task implemented.
Task 6 complete: ILI9341 remains SPI2 DMA double-buffered; offline UI and optional LED/buzzer alarm component build successfully.
Verification limitation: no native GCC/Clang is installed, so pure C behavior binaries cannot execute on this workstation. Test sources are cross-compiled; PowerShell contracts and complete target builds provide available verification.
Task 7 complete: all PowerShell contracts pass; pure C tests compile/link for ARM; STM32 image is 23,792 B text / 96 B data / 11,320 B BSS; ESP32-S3 uses 82,800 B RAM and 614,893 B flash. Hardware smoke test remains the required on-device validation.
