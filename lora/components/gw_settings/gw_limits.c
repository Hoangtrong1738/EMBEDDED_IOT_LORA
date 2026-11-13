#include "gw_limits.h"

static const int min_values[LIM_COUNT] = { 200, 5, 100, 20, 30, 0 };
static const int max_values[LIM_COUNT] = { 300, 300, 6000, 100, 100, 1 };
static const int step_values[LIM_COUNT] = { 5, 5, 50, 1, 5, 1 };

static int clamp(gw_limit_id_t id, int value)
{
    if (id >= LIM_COUNT) return value;
    if (value < min_values[id]) return min_values[id];
    if (value > max_values[id]) return max_values[id];
    return value;
}

void gw_limits_defaults(gw_limits_t *limits)
{
    limits->volt_max = 250U; limits->amp_max_x10 = 100U; limits->watt_max = 2000U;
    limits->temp_max = 60U; limits->hum_max = 90U; limits->buzzer = 1U;
}

int gw_limit_get(const gw_limits_t *limits, gw_limit_id_t id)
{
    switch (id) {
    case LIM_VOLT: return limits->volt_max; case LIM_AMP: return limits->amp_max_x10;
    case LIM_WATT: return limits->watt_max; case LIM_TEMP: return limits->temp_max;
    case LIM_HUM: return limits->hum_max; case LIM_BUZZER: return limits->buzzer;
    default: return 0;
    }
}

void gw_limit_set(gw_limits_t *limits, gw_limit_id_t id, int value)
{
    value = clamp(id, value);
    switch (id) {
    case LIM_VOLT: limits->volt_max = (uint16_t)value; break;
    case LIM_AMP: limits->amp_max_x10 = (uint16_t)value; break;
    case LIM_WATT: limits->watt_max = (uint16_t)value; break;
    case LIM_TEMP: limits->temp_max = (uint16_t)value; break;
    case LIM_HUM: limits->hum_max = (uint16_t)value; break;
    case LIM_BUZZER: limits->buzzer = (uint8_t)value; break;
    default: break;
    }
}

void gw_limits_clamp(gw_limits_t *limits)
{
    for (int id = LIM_VOLT; id < LIM_COUNT; ++id) gw_limit_set(limits, (gw_limit_id_t)id, gw_limit_get(limits, (gw_limit_id_t)id));
}

bool gw_limits_equal(const gw_limits_t *left, const gw_limits_t *right)
{
    for (int id = LIM_VOLT; id < LIM_COUNT; ++id)
        if (gw_limit_get(left, (gw_limit_id_t)id) != gw_limit_get(right, (gw_limit_id_t)id)) return false;
    return true;
}

int gw_limit_step(gw_limit_id_t id, int value, int direction)
{
    if (id == LIM_BUZZER) return value ? 0 : 1;
    return clamp(id, value + (direction >= 0 ? step_values[id] : -step_values[id]));
}
