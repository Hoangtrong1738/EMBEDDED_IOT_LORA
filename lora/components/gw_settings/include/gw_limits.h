#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t volt_max;
    uint16_t amp_max_x10;
    uint16_t watt_max;
    uint16_t temp_max;
    uint16_t hum_max;
    uint8_t buzzer;
} gw_limits_t;

typedef enum { LIM_VOLT, LIM_AMP, LIM_WATT, LIM_TEMP, LIM_HUM, LIM_BUZZER, LIM_COUNT } gw_limit_id_t;

void gw_limits_defaults(gw_limits_t *limits);
void gw_limits_clamp(gw_limits_t *limits);
bool gw_limits_equal(const gw_limits_t *left, const gw_limits_t *right);
int gw_limit_get(const gw_limits_t *limits, gw_limit_id_t id);
void gw_limit_set(gw_limits_t *limits, gw_limit_id_t id, int value);
int gw_limit_step(gw_limit_id_t id, int value, int direction);
