#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "dht11.h"
#include "iwdg.h"
#include "LoRa.h"
#include "lora_data.h"
#include "node_lora.h"
#include "node_board_config.h"
#include "node_state.h"
#include "spi.h"
#include "usart.h"

#define NODE_ACK_TIMEOUT_MS       1500U
#define NODE_TX_PERIOD_MS        10000U
#define NODE_SENSOR_PERIOD_MS     1000U
#define NODE_RX_BUFFER_SIZE         32U

enum { TX_REASON_PERIODIC = 1U, TX_REASON_STATE_CHANGE = 2U };

static LoRa node_radio;
static node_state_t node_state;
static SemaphoreHandle_t state_mutex;
static QueueHandle_t tx_queue;
static TaskHandle_t radio_task_handle;
static TaskHandle_t monitor_task_handle;
static uint16_t packet_sequence;

static void uart_log(const char *message)
{
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)message,
                            (uint16_t)strlen(message), 1000U);
}

static void set_leds(uint8_t system_ready, uint8_t acknowledged, uint8_t status)
{
    HAL_GPIO_WritePin(NODE_LED_SYSTEM_PORT, NODE_LED_SYSTEM_PIN,
                      system_ready ? NODE_LED_ACTIVE_STATE
                                   : NODE_LED_IDLE_STATE);
    HAL_GPIO_WritePin(NODE_LED_LINK_PORT, NODE_LED_LINK_PIN,
                      acknowledged ? NODE_LED_ACTIVE_STATE
                                   : NODE_LED_IDLE_STATE);
    HAL_GPIO_WritePin(NODE_LED_ALARM_PORT, NODE_LED_ALARM_PIN,
                      status != 0U ? NODE_LED_ACTIVE_STATE
                                   : NODE_LED_IDLE_STATE);
#if NODE_BUZZER_ENABLED
    HAL_GPIO_WritePin(NODE_BUZZER_PORT, NODE_BUZZER_PIN,
                      status != 0U ? NODE_BUZZER_ACTIVE_STATE
                                   : NODE_BUZZER_IDLE_STATE);
#endif
}

static void queue_telemetry(uint8_t reason)
{
    if (tx_queue != 0) (void)xQueueSend(tx_queue, &reason, 0U);
}

static uint8_t radio_init(void)
{
    node_radio = newLoRa();
    node_radio.CS_port = GPIOA;
    node_radio.CS_pin = GPIO_PIN_4;
    node_radio.reset_port = GPIOB;
    node_radio.reset_pin = GPIO_PIN_0;
    node_radio.DIO0_port = GPIOB;
    node_radio.DIO0_pin = GPIO_PIN_1;
    node_radio.hSPIx = &hspi1;
    node_radio.frequency = 433;
    node_radio.spreadingFactor = LORA_SF_7;
    node_radio.bandwidth = LORA_BW_125_KHZ;
    node_radio.codingRate = LORA_CR_4_5;
    node_radio.preamble = 8;
    node_radio.power = LORA_POWER_17DBM;
    node_radio.overCurrentProtection = 100;
    if (LoRa_init(&node_radio) != LORA_OK) return 0U;
    LoRa_setSyncWord(&node_radio, 0x12U);
    LoRa_startReceiving(&node_radio);
    return 1U;
}

static void create_telemetry_packet(lora_data_pkt_t *packet)
{
    node_snapshot_t sample;
    (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
    node_state_snapshot(&node_state, &sample);
    (void)xSemaphoreGive(state_mutex);

    memset(packet, 0, sizeof(*packet));
    packet->magic = PKT_MAGIC;
    packet->node_id = Node_ID;
    packet->seq = packet_sequence++;
    packet->volt = sample.voltage_v;
    packet->mA = sample.current_ma;
    packet->pwr = sample.power_mw;
    packet->Wh = sample.energy_wh;
    packet->temp = sample.temperature_c;
    packet->hum = sample.humidity_percent;
    packet->status = sample.status;
    packet->crc16 = crc16_modbus((const u8 *)packet,
                                 (u8)(sizeof(*packet) - sizeof(packet->crc16)));
}

static void transmit_control(const uint8_t *frame, uint8_t length)
{
    if (length > 0U) {
        (void)LoRa_transmit(&node_radio, (uint8_t *)frame, length, 1000U);
        LoRa_startReceiving(&node_radio);
    }
}

static void handle_config(const uint8_t *frame, uint8_t length)
{
    node_config_cmd_t command;
    node_limits_t limits;
    uint8_t reply[NODE_CONTROL_REPLY_SIZE];
    uint8_t changed;

    if (!lora_decode_config(frame, length, &command)) return;
    limits.voltage_limit_v = command.voltage_limit;
    limits.current_limit_ma = command.current_limit_ma;
    limits.power_limit_mw = command.power_limit_mw;
    limits.temperature_limit_c = command.temp_limit;
    limits.humidity_limit_percent = command.hum_limit;
    (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
    node_state_apply_limits(&node_state, &limits);
    changed = node_state_evaluate(&node_state);
    (void)xSemaphoreGive(state_mutex);
    if (changed) queue_telemetry(TX_REASON_STATE_CHANGE);
    transmit_control(reply, lora_encode_control_reply(reply, sizeof(reply),
                                                       MSG_NODE_CONFIG_ACK,
                                                       command.seq,
                                                       NODE_REPLY_OK));
    uart_log("Node limits updated\r\n");
}

static void handle_heartbeat(const uint8_t *frame, uint8_t length)
{
    node_heartbeat_t heartbeat;
    uint8_t reply[NODE_HEARTBEAT_FRAME_SIZE];
    if (!lora_decode_heartbeat(frame, length, MSG_NODE_HEARTBEAT, &heartbeat)) return;
    transmit_control(reply, lora_encode_heartbeat(reply, sizeof(reply),
                                                   MSG_NODE_HEARTBEAT_ACK,
                                                   heartbeat.seq));
}

static uint8_t process_received_frame(const uint8_t *frame, uint8_t length,
                                      uint16_t expected_ack)
{
    if (length == sizeof(ack_t)) {
        ack_t ack;
        memcpy(&ack, frame, sizeof(ack));
        if (ack.magic == PKT_MAGIC && ack.node_id == Node_ID &&
            ack.seq == expected_ack) return 1U;
    } else if (length == NODE_CONFIG_FRAME_SIZE) {
        handle_config(frame, length);
    } else if (length == NODE_HEARTBEAT_FRAME_SIZE) {
        handle_heartbeat(frame, length);
    }
    return 0U;
}

static uint8_t wait_for_ack(uint16_t expected_sequence)
{
    uint8_t frame[NODE_RX_BUFFER_SIZE];
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(NODE_ACK_TIMEOUT_MS);
    LoRa_startReceiving(&node_radio);
    while ((xTaskGetTickCount() - start) < timeout) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        (void)ulTaskNotifyTake(pdTRUE, timeout - elapsed);
        {
            const uint8_t length = LoRa_receive(&node_radio, frame, sizeof(frame));
            if (length > 0U && process_received_frame(frame, length,
                                                       expected_sequence)) return 1U;
        }
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
    return 0U;
}

static uint8_t send_with_ack(const lora_data_pkt_t *packet)
{
    uint8_t attempt;
    for (attempt = 1U; attempt <= ACK_RETRIES; attempt++) {
        if (LoRa_transmit(&node_radio, (uint8_t *)packet,
                          (uint8_t)sizeof(*packet), 1000U) &&
            wait_for_ack(packet->seq)) return 1U;
        LoRa_startReceiving(&node_radio);
    }
    return 0U;
}

static void dht_task(void *argument)
{
    TickType_t wake = xTaskGetTickCount();
    (void)argument;
    for (;;) {
        dht11_data_t data = {0};
        const uint8_t valid = (uint8_t)(dht11_read(&data) == 0);
        (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
        node_state_update_dht(&node_state, valid, (i16)data.temp, data.hum);
        (void)xSemaphoreGive(state_mutex);
        xTaskNotifyGive(monitor_task_handle);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(NODE_SENSOR_PERIOD_MS));
    }
}

static void power_task(void *argument)
{
    TickType_t wake = xTaskGetTickCount();
    (void)argument;
    for (;;) {
        (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
        /* Placeholder until the real meter is fitted: zero is valid data. */
        node_state_update_power(&node_state, 0U, 0U, 0U, 0U, 1U);
        (void)xSemaphoreGive(state_mutex);
        xTaskNotifyGive(monitor_task_handle);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(NODE_SENSOR_PERIOD_MS));
    }
}

static void monitor_task(void *argument)
{
    TickType_t last_periodic = xTaskGetTickCount();
    (void)argument;
    for (;;) {
        uint8_t changed;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(NODE_SENSOR_PERIOD_MS));
        (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
        changed = node_state_evaluate(&node_state);
        (void)xSemaphoreGive(state_mutex);
        if (changed) queue_telemetry(TX_REASON_STATE_CHANGE);
        if ((xTaskGetTickCount() - last_periodic) >=
            pdMS_TO_TICKS(NODE_TX_PERIOD_MS)) {
            last_periodic = xTaskGetTickCount();
            queue_telemetry(TX_REASON_PERIODIC);
        }
    }
}

static void radio_task(void *argument)
{
    uint8_t frame[NODE_RX_BUFFER_SIZE];
    uint8_t reason;
    (void)argument;
    set_leds(0U, 0U, 0U);
    while (!radio_init()) {
        HAL_GPIO_TogglePin(NODE_LED_ALARM_PORT, NODE_LED_ALARM_PIN);
        uart_log("SX127x init failed\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000U));
    }
    set_leds(1U, 0U, 0U);
    uart_log("Node 1 offline monitor ready\r\n");

    for (;;) {
        if (xQueueReceive(tx_queue, &reason, 0U) == pdTRUE) {
            lora_data_pkt_t packet;
            node_snapshot_t sample;
            uint8_t acknowledged;
            char message[112];
            (void)reason;
            create_telemetry_packet(&packet);
            acknowledged = send_with_ack(&packet);
            (void)xSemaphoreTake(state_mutex, portMAX_DELAY);
            node_state_snapshot(&node_state, &sample);
            (void)xSemaphoreGive(state_mutex);
            set_leds(1U, acknowledged, sample.status);
            (void)snprintf(message, sizeof(message),
                           "TX seq=%u T=%dC H=%u%% status=0x%02X %s\r\n",
                           packet.seq, packet.temp, packet.hum, packet.status,
                           acknowledged ? "ACK" : "NO_ACK");
            uart_log(message);
        }
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50U)) > 0U) {
            const uint8_t length = LoRa_receive(&node_radio, frame, sizeof(frame));
            if (length > 0U) (void)process_received_frame(frame, length, 0xFFFFU);
        }
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}

void node_lora_dio0_isr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (radio_task_handle != 0) {
        vTaskNotifyGiveFromISR(radio_task_handle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

uint8_t node_init(void)
{
    node_state_init(&node_state);
    state_mutex = xSemaphoreCreateMutex();
    tx_queue = xQueueCreate(4U, sizeof(uint8_t));
    if (state_mutex == 0 || tx_queue == 0) return 0U;

    if (xTaskCreate(monitor_task, "monitor", 192U, 0,
                    tskIDLE_PRIORITY + 2U, &monitor_task_handle) != pdPASS ||
        xTaskCreate(dht_task, "dht", 192U, 0,
                    tskIDLE_PRIORITY + 2U, 0) != pdPASS ||
        xTaskCreate(power_task, "power", 160U, 0,
                    tskIDLE_PRIORITY + 1U, 0) != pdPASS ||
        xTaskCreate(radio_task, "radio", 512U, 0,
                    tskIDLE_PRIORITY + 3U, &radio_task_handle) != pdPASS) return 0U;
    return 1U;
}
