#include "gateway_alarm.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_err.h"

static bool s_node_online;
static bool s_alert_active;
static bool s_link_led_on;
static bool s_alarm_led_on;
static uint32_t s_last_toggle_ms;

bool gateway_alarm_should_sound(bool node_online, uint8_t alert_mask,
                                bool buzzer_enabled)
{
    /* Keep link-loss reporting visual; sound only for an online node alarm. */
    return buzzer_enabled && node_online && alert_mask != 0U;
}

esp_err_t gateway_alarm_init(void)
{
    s_node_online = false;
    s_alert_active = false;
    s_link_led_on = false;
    s_alarm_led_on = false;
    s_last_toggle_ms = 0U;

    gpio_reset_pin(GATEWAY_LED_SYSTEM_PIN);
    gpio_set_direction(GATEWAY_LED_SYSTEM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GATEWAY_LED_SYSTEM_PIN, GATEWAY_LED_IDLE_LEVEL);

    gpio_reset_pin(GATEWAY_LED_LINK_PIN);
    gpio_set_direction(GATEWAY_LED_LINK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GATEWAY_LED_LINK_PIN, GATEWAY_LED_IDLE_LEVEL);

    gpio_reset_pin(GATEWAY_LED_ALARM_PIN);
    gpio_set_direction(GATEWAY_LED_ALARM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GATEWAY_LED_ALARM_PIN, GATEWAY_LED_IDLE_LEVEL);

#if GATEWAY_BUZZER_PIN >= 0
    gpio_reset_pin(GATEWAY_BUZZER_PIN);
    gpio_set_direction(GATEWAY_BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GATEWAY_BUZZER_PIN, 0);
#endif
    return ESP_OK;
}

void gateway_alarm_update(bool node_online, uint8_t alert_mask,
                          bool buzzer_enabled, uint32_t now_ms)
{
    const bool alert_active = node_online && alert_mask != 0U;
    if (node_online != s_node_online || alert_active != s_alert_active) {
        s_node_online = node_online;
        s_alert_active = alert_active;
        s_link_led_on = node_online;
        s_alarm_led_on = alert_active;
        s_last_toggle_ms = now_ms;
    }

#if GATEWAY_BUZZER_PIN >= 0
    gpio_set_level(GATEWAY_BUZZER_PIN,
                   gateway_alarm_should_sound(node_online, alert_mask,
                                               buzzer_enabled) ? 1 : 0);
#else
    (void)buzzer_enabled;
#endif
}

void gateway_alarm_tick(uint32_t now_ms)
{
    const uint32_t elapsed_ms = (uint32_t)(now_ms - s_last_toggle_ms);
    const uint32_t toggle_count = elapsed_ms / GATEWAY_LED_BLINK_MS;

    if (toggle_count > 0U) {
        if ((toggle_count & 1U) != 0U) {
            s_link_led_on = !s_link_led_on;
            s_alarm_led_on = !s_alarm_led_on;
        }
        s_last_toggle_ms += toggle_count * GATEWAY_LED_BLINK_MS;
    }

    gpio_set_level(GATEWAY_LED_LINK_PIN,
                   s_node_online || s_link_led_on
                       ? GATEWAY_LED_ACTIVE_LEVEL : GATEWAY_LED_IDLE_LEVEL);
    gpio_set_level(GATEWAY_LED_ALARM_PIN,
                   s_alert_active && s_alarm_led_on
                       ? GATEWAY_LED_ACTIVE_LEVEL : GATEWAY_LED_IDLE_LEVEL);
}
