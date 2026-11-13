# Offline Node 1 LoRa System Design

## Intent

Build a stable offline monitoring system before adding MQTT or cellular publishing. One STM32F103 node measures DHT11 data, keeps a one-second placeholder power task, evaluates gateway-provided limits, and exchanges reliable LoRa messages with one ESP32-S3 gateway. The gateway displays the latest data on its existing ILI9341/LVGL DMA UI and owns link supervision and user-visible alarms.

## Scope

- Exactly one node, `node_id = 1`.
- DHT11 is sampled every 1 second on STM32 PB3.
- A power task runs every 1 second and publishes zero voltage, current, power, and energy until the real meter is added.
- The node sends its latest telemetry every 10 seconds and immediately when its alarm/fault state changes.
- Gateway limits are sent to the node, CRC-checked, applied immediately, and acknowledged.
- Gateway heartbeat requests verify the node is still listening between telemetry transmissions.
- Both radios have one owning task. DIO0 interrupt handlers only notify those tasks; they never perform SPI transactions.
- The gateway retains the existing ILI9341 SPI2 DMA, double-buffered LVGL path.
- MQTT, 4G, cloud storage, and multi-node UI/state are out of scope.

## Architecture

### STM32 node

`node_state` is the pure state/limit/alarm layer. `dht_task` and `power_task` update it once per second. `monitor_task` snapshots state, detects alarm/fault transitions, and queues immediate telemetry. A ten-second software timer queues periodic telemetry. `radio_task` is the only SX127x owner and handles telemetry transmission, ACK/retry, configuration, heartbeat replies, and DIO0 task notifications.

### ESP32-S3 gateway

`gateway_radio_task` is the only SX127x owner. It receives telemetry, sends ACKs, sends pending settings, and emits heartbeat requests. `gateway_process_task` owns the latest Node 1 record, sequence/loss accounting, link age, and alarm mask. The existing display component receives overwrite-only updates so slow rendering cannot back up radio processing. `gateway_alarm` maps logical link/alarm state to optional configurable LED/buzzer GPIOs.

## Protocol

The existing 25-byte telemetry and 4-byte ACK remain compatible. Control frames use `magic`, `type`, `node_id`, little-endian sequence, typed payload, and CRC16-Modbus. Control types are config (`0xC1`), config ACK (`0xC2`), heartbeat (`0xC3`), and heartbeat ACK (`0xC4`). Config values use volts, milliamps, milliwatts, degrees C, and percent RH.

## Reliability rules

- Telemetry retries up to three times with the same sequence.
- Duplicate telemetry is ACKed but not rendered twice.
- An alarm transition queues one immediate packet; a continuously high reading does not flood the channel.
- Node retains the last valid DHT11 sample and asserts `ST_DHT_FAIL` after a failed read.
- Gateway marks Node 1 offline after 15 seconds without telemetry or heartbeat response.
- Control frames with a wrong length, node ID, type, or CRC are ignored.
- Hardware alarm GPIOs default disabled until actual product pins are supplied.

## Approaches considered

1. A single loop/task per device is smallest but blocks display, sensor sampling, and downlink reception during ACK waits.
2. Fully separated tasks with unrestricted radio calls are simple initially but introduce SPI races and unpredictable half-duplex behavior.
3. Selected: event-driven tasks with one radio owner and queues/notifications. This adds small RTOS plumbing but makes timing and ownership deterministic.

