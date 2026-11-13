#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gateway_protocol.h"
#include "gateway_state.h"
#include "gateway_alarm.h"
#include "gw_limits.h"
#include "lora.h"

#define ACK_PACKET_SIZE        4U
#define RADIO_QUEUE_LENGTH     8U
#define CONTROL_QUEUE_LENGTH   8U
#define UART_QUEUE_LENGTH      8U
#define CONFIG_RETRY_MS     5000U
#define CONTROL_RESPONSE_WINDOW_MS 100U

static const char *TAG = "gateway";

typedef enum {
    RADIO_EVENT_TELEMETRY,
    RADIO_EVENT_CONFIG_ACK,
    RADIO_EVENT_HEARTBEAT_ACK,
} radio_event_type_t;

typedef struct {
    radio_event_type_t type;
    gw_telemetry_t telemetry;
    uint16_t sequence;
    uint8_t status;
    int rssi;
    float snr;
} radio_event_t;

typedef enum {
    RADIO_CONTROL_CONFIG,
    RADIO_CONTROL_HEARTBEAT,
} radio_control_type_t;

typedef struct {
    radio_control_type_t type;
    uint16_t sequence;
    gw_limits_t limits;
} radio_control_t;

typedef struct {
    gw_telemetry_t telemetry;
} uart_frame_event_t;

static QueueHandle_t s_radio_events;
static QueueHandle_t s_radio_controls;
static QueueHandle_t s_uart_frames;
static TaskHandle_t s_radio_task;
static bool s_uart_ready;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void IRAM_ATTR lora_dio0_isr(void *argument)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    (void)argument;
    if (s_radio_task != NULL) {
        vTaskNotifyGiveFromISR(s_radio_task, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

static esp_err_t init_dio0_interrupt(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << LORA_DIO0_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    esp_err_t error = gpio_config(&config);
    if (error != ESP_OK) return error;
    error = gpio_install_isr_service(0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
    return gpio_isr_handler_add(LORA_DIO0_PIN, lora_dio0_isr, NULL);
}

static esp_err_t init_gateway_uart(void)
{
    const uart_config_t config = {
        .baud_rate = GATEWAY_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t error = uart_driver_install(UART_NUM_0, 256U, 512U, 0U, NULL, 0U);
    if (error != ESP_OK) return error;

    error = uart_param_config(UART_NUM_0, &config);
    if (error == ESP_OK) {
        error = uart_set_pin(UART_NUM_0, GATEWAY_UART_TX_PIN,
                             GATEWAY_UART_RX_PIN,
                             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (error != ESP_OK) {
        (void)uart_driver_delete(UART_NUM_0);
    }
    return error;
}

static void init_radio(void)
{
    lora_set_frequency(433000000L);
    lora_set_spreading_factor(7);
    lora_set_bandwidth(7);
    lora_set_coding_rate(1);
    lora_set_preamble_length(8);
    lora_set_sync_word(0x12);
    lora_enable_crc();
    lora_receive();
}

static void send_control(const radio_control_t *control)
{
    uint8_t frame[GW_CONFIG_FRAME_SIZE];
    size_t length = 0U;
    if (control->type == RADIO_CONTROL_CONFIG) {
        length = gw_encode_config(frame, sizeof(frame), control->sequence,
                                  &control->limits);
    } else if (control->type == RADIO_CONTROL_HEARTBEAT) {
        length = gw_encode_heartbeat(frame, sizeof(frame), GW_MSG_HEARTBEAT,
                                     control->sequence);
    }
    if (length > 0U) {
        lora_send_packet(frame, (int)length);
        lora_receive();
    }
}

static void gateway_uart_task(void *argument)
{
    uart_frame_event_t event;
    gw_rx_state_t forwarded = {0};
    (void)argument;

    for (;;) {
        if (xQueueReceive(s_uart_frames, &event, portMAX_DELAY) != pdTRUE) continue;
        if (gw_accept_sequence(&forwarded, event.telemetry.seq) != GW_SEQUENCE_NEW) {
            ESP_LOGD(TAG, "Skipped duplicate or stale UART frame seq=%u",
                     (unsigned)event.telemetry.seq);
            continue;
        }

        char line[96];
        const gw_telemetry_t *telemetry = &event.telemetry;
        const int line_length = snprintf(
            line, sizeof(line), "TLM,%u,%u,%u,%lu,%lu,%lu,%d,%u,0x%02X\r\n",
            (unsigned)telemetry->node_id, (unsigned)telemetry->seq,
            (unsigned)telemetry->voltage_v,
            (unsigned long)telemetry->current_ma,
            (unsigned long)telemetry->power_mw,
            (unsigned long)telemetry->energy_wh,
            (int)telemetry->temperature_c,
            (unsigned)telemetry->humidity_percent,
            (unsigned)telemetry->status);
        if (line_length < 0 || (size_t)line_length >= sizeof(line)) {
            ESP_LOGE(TAG, "Failed to format UART0 telemetry seq=%u",
                     (unsigned)telemetry->seq);
            continue;
        }

        const int written = uart_write_bytes(UART_NUM_0, line,
                                             (size_t)line_length);
        if (written != line_length) {
            ESP_LOGE(TAG, "UART0 telemetry forwarding incomplete: %d/%d",
                     written, line_length);
        }
    }
}

static void receive_radio_frame(void)
{
    uint8_t packet[64];
    const int length = lora_receive_packet(packet, sizeof(packet));
    radio_event_t event = {0};

    if (length <= 0) {
        lora_receive();
        return;
    }
    if (gw_decode_packet(packet, (size_t)length, &event.telemetry)) {
        uint8_t ack[ACK_PACKET_SIZE] = {
            GW_PACKET_MAGIC, event.telemetry.node_id,
            (uint8_t)event.telemetry.seq,
            (uint8_t)(event.telemetry.seq >> 8U),
        };
        event.type = RADIO_EVENT_TELEMETRY;
        event.rssi = lora_packet_rssi();
        event.snr = lora_packet_snr();
        if (s_uart_ready) {
            const uart_frame_event_t uart_event = {
                .telemetry = event.telemetry,
            };
            if (xQueueSend(s_uart_frames, &uart_event, 0U) != pdTRUE) {
                ESP_LOGW(TAG, "UART queue full; dropping telemetry seq=%u",
                         (unsigned)uart_event.telemetry.seq);
            }
        }
        lora_send_packet(ack, sizeof(ack));
        lora_receive();
        (void)xQueueSend(s_radio_events, &event, 0U);
        return;
    }
    {
        gw_control_ack_t ack;
        if (gw_decode_control_ack(packet, (size_t)length,
                                  GW_MSG_CONFIG_ACK, &ack)) {
            event.type = RADIO_EVENT_CONFIG_ACK;
            event.sequence = ack.seq;
            event.status = ack.status;
            (void)xQueueSend(s_radio_events, &event, 0U);
            lora_receive();
            return;
        }
    }
    {
        gw_heartbeat_t heartbeat;
        if (gw_decode_heartbeat(packet, (size_t)length,
                                GW_MSG_HEARTBEAT_ACK, &heartbeat)) {
            event.type = RADIO_EVENT_HEARTBEAT_ACK;
            event.sequence = heartbeat.seq;
            (void)xQueueSend(s_radio_events, &event, 0U);
            lora_receive();
            return;
        }
    }
    ESP_LOGW(TAG, "Rejected LoRa frame length=%d", length);
    lora_receive();
}

static void gateway_radio_task(void *argument)
{
    radio_control_t control;
    (void)argument;
    if (!lora_init()) {
        ESP_LOGE(TAG, "SX127x init failed");
        vTaskDelete(NULL);
        return;
    }
    init_radio();
    ESP_LOGI(TAG, "LoRa ready: 433 MHz SF7 BW125 CR4/5 DIO0 GPIO%d",
             LORA_DIO0_PIN);

    for (;;) {
        TickType_t receive_window = pdMS_TO_TICKS(20U);
        if (xQueueReceive(s_radio_controls, &control, 0U) == pdTRUE) {
            send_control(&control);
            /* Do not transmit two control frames back-to-back: the node needs
             * enough airtime to send its ACK before the next gateway TX. */
            receive_window = pdMS_TO_TICKS(CONTROL_RESPONSE_WINDOW_MS);
        }
        (void)ulTaskNotifyTake(pdTRUE, receive_window);
        if (lora_received()) receive_radio_frame();
    }
}

static void queue_config(const gateway_state_t *state, const gw_limits_t *limits)
{
    radio_control_t control = {.type = RADIO_CONTROL_CONFIG, .limits = *limits};
    if (gateway_state_config_pending(state, &control.sequence))
        (void)xQueueSend(s_radio_controls, &control, 0U);
}

static void gateway_process_task(void *argument)
{
    gateway_state_t state;
    gw_limits_t limits;
    radio_event_t event;
    uint32_t last_config_send = 0U;
    bool previous_online = false;
    bool offline_announced = false;
    uint8_t last_alert_mask = 0U;
    (void)argument;

    gateway_state_init(&state, now_ms());
    gw_limits_defaults(&limits);
    (void)gateway_state_queue_config(&state);
    gateway_alarm_update(false, 0U, limits.buzzer != 0U, now_ms());

    for (;;) {
        const uint32_t now = now_ms();
        if (xQueueReceive(s_radio_events, &event, pdMS_TO_TICKS(100U)) == pdTRUE) {
            if (event.type == RADIO_EVENT_TELEMETRY) {
                const gw_sequence_result_t result = gateway_state_on_telemetry(
                    &state, event.telemetry.seq, now);
                if (result == GW_SEQUENCE_NEW) {
                    last_alert_mask = gw_alert_mask_with_limits(
                        &event.telemetry, &limits);
                    gateway_alarm_update(true, last_alert_mask,
                                         limits.buzzer != 0U, now);
                }
            } else if (event.type == RADIO_EVENT_CONFIG_ACK &&
                       event.status == GW_REPLY_OK) {
                (void)gateway_state_accept_config_ack(&state, event.sequence);
            } else if (event.type == RADIO_EVENT_HEARTBEAT_ACK) {
                (void)gateway_state_accept_heartbeat(&state, event.sequence, now);
            }
        }

        if (gateway_state_heartbeat_due(&state, now)) {
            radio_control_t control = {
                .type = RADIO_CONTROL_HEARTBEAT,
                .sequence = gateway_state_begin_heartbeat(&state, now),
            };
            (void)xQueueSend(s_radio_controls, &control, 0U);
        }
        if (gateway_state_config_pending(&state, NULL) &&
            (uint32_t)(now - last_config_send) >= CONFIG_RETRY_MS) {
            queue_config(&state, &limits);
            last_config_send = now;
        }
        {
            const bool online = gateway_state_online(&state, now);
            if (online != previous_online) {
                ESP_LOGI(TAG, "Node 1 is %s", online ? "ONLINE" : "OFFLINE");
                gateway_alarm_update(online, last_alert_mask,
                                     limits.buzzer != 0U, now);
                offline_announced = !online;
                previous_online = online;
            } else if (!online && !offline_announced &&
                       gateway_state_offline_due(&state, now)) {
                ESP_LOGI(TAG, "Node 1 is OFFLINE");
                gateway_alarm_update(false, last_alert_mask,
                                     limits.buzzer != 0U, now);
                offline_announced = true;
            }
        }
        gateway_alarm_tick(now);
    }
}

void app_main(void)
{
    const esp_err_t uart_error = init_gateway_uart();
    s_uart_ready = uart_error == ESP_OK;
    if (!s_uart_ready) {
        ESP_LOGE(TAG, "UART0 forwarding unavailable: %s",
                 esp_err_to_name(uart_error));
    }

    const esp_err_t alarm_error = gateway_alarm_init();
    if (alarm_error != ESP_OK) {
        ESP_LOGE(TAG, "LED/buzzer startup failed: %s",
                 esp_err_to_name(alarm_error));
    }

    /* Start independent forwarding and radio/link supervision tasks. */
    s_radio_events = xQueueCreate(RADIO_QUEUE_LENGTH, sizeof(radio_event_t));
    s_radio_controls = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(radio_control_t));
    s_uart_frames = xQueueCreate(UART_QUEUE_LENGTH, sizeof(uart_frame_event_t));
    if (s_radio_events == NULL || s_radio_controls == NULL ||
        s_uart_frames == NULL) {
        ESP_LOGE(TAG, "Gateway queue allocation failed");
        return;
    }
    if (s_uart_ready &&
        xTaskCreate(gateway_uart_task, "gateway_uart", 3072U, NULL, 4U,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "UART task creation failed; forwarding disabled");
        s_uart_ready = false;
    }
    if (xTaskCreatePinnedToCore(gateway_radio_task, "gateway_radio", 4096U,
                                NULL, 5U, &s_radio_task, 0) != pdPASS ||
        xTaskCreatePinnedToCore(gateway_process_task, "gateway_process", 4096U,
                                NULL, 4U, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Gateway task creation failed");
        return;
    }
    if (init_dio0_interrupt() != ESP_OK) {
        ESP_LOGW(TAG, "DIO0 interrupt unavailable; 20 ms radio fallback active");
    }
}
