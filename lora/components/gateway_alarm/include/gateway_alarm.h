#pragma once

#include <stdbool.h>
#include <stdint.h>

int gateway_alarm_init(void);
bool gateway_alarm_should_sound(bool node_online, uint8_t alert_mask,
                                bool buzzer_enabled);
void gateway_alarm_update(bool node_online, uint8_t alert_mask,
                          bool buzzer_enabled, uint32_t now_ms);
void gateway_alarm_tick(uint32_t now_ms);
