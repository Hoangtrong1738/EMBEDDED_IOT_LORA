#ifndef NODE_STATE_H
#define NODE_STATE_H

#include "lora_data.h"

typedef struct {
    u16 voltage_limit_v;
    u32 current_limit_ma;
    u32 power_limit_mw;
    i16 temperature_limit_c;
    u16 humidity_limit_percent;
} node_limits_t;

typedef struct {
    u16 voltage_v;
    u32 current_ma;
    u32 power_mw;
    u32 energy_wh;
    i16 temperature_c;
    u16 humidity_percent;
    u8 status;
} node_snapshot_t;

typedef struct {
    node_snapshot_t sample;
    node_limits_t limits;
    u8 published_status;
} node_state_t;

void node_state_init(node_state_t *state);
void node_state_update_dht(node_state_t *state, u8 valid, i16 temperature_c,
                           u16 humidity_percent);
void node_state_update_power(node_state_t *state, u16 voltage_v,
                             u32 current_ma, u32 power_mw, u32 energy_wh,
                             u8 valid);
void node_state_apply_limits(node_state_t *state, const node_limits_t *limits);
u8 node_state_evaluate(node_state_t *state);
void node_state_snapshot(const node_state_t *state, node_snapshot_t *snapshot);

#endif
