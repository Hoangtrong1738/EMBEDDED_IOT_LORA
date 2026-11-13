#include "node_state.h"

#include <string.h>

void node_state_init(node_state_t *state)
{
    if (state == 0) return;
    memset(state, 0, sizeof(*state));
    state->limits.voltage_limit_v = 250U;
    state->limits.current_limit_ma = 10000U;
    state->limits.power_limit_mw = 2000000U;
    state->limits.temperature_limit_c = 60;
    state->limits.humidity_limit_percent = 90U;
    state->sample.status = ST_DHT_FAIL | ST_PZEM_FAIL;
    state->published_status = state->sample.status;
}

void node_state_update_dht(node_state_t *state, u8 valid, i16 temperature_c,
                           u16 humidity_percent)
{
    if (state == 0) return;
    if (valid) {
        state->sample.temperature_c = temperature_c;
        state->sample.humidity_percent = humidity_percent;
        state->sample.status &= (u8)~ST_DHT_FAIL;
    } else {
        state->sample.status |= ST_DHT_FAIL;
    }
}

void node_state_update_power(node_state_t *state, u16 voltage_v,
                             u32 current_ma, u32 power_mw, u32 energy_wh,
                             u8 valid)
{
    if (state == 0) return;
    state->sample.voltage_v = voltage_v;
    state->sample.current_ma = current_ma;
    state->sample.power_mw = power_mw;
    state->sample.energy_wh = energy_wh;
    if (valid) state->sample.status &= (u8)~ST_PZEM_FAIL;
    else state->sample.status |= ST_PZEM_FAIL;
}

void node_state_apply_limits(node_state_t *state, const node_limits_t *limits)
{
    if (state == 0 || limits == 0) return;
    state->limits = *limits;
}

u8 node_state_evaluate(node_state_t *state)
{
    u8 next;
    u8 changed;
    if (state == 0) return 0U;

    next = state->sample.status &
           (ST_PZEM_FAIL | ST_WDG_RESET | ST_DHT_FAIL);
    if (state->sample.voltage_v >= state->limits.voltage_limit_v)
        next |= ST_OVER_VOLTAGE;
    if (state->sample.current_ma >= state->limits.current_limit_ma)
        next |= ST_OVER_CURRENT;
    if (state->sample.power_mw >= state->limits.power_limit_mw)
        next |= ST_OVER_POWER;
    if (state->sample.temperature_c >= state->limits.temperature_limit_c)
        next |= ST_OVER_TEMP;
    if (state->sample.humidity_percent >= state->limits.humidity_limit_percent)
        next |= ST_OVER_HUMIDITY;

    state->sample.status = next;
    changed = (u8)(next != state->published_status);
    state->published_status = next;
    return changed;
}

void node_state_snapshot(const node_state_t *state, node_snapshot_t *snapshot)
{
    if (state == 0 || snapshot == 0) return;
    *snapshot = state->sample;
}
