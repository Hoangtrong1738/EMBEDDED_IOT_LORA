#include "gateway_state.h"

#include <string.h>

void gateway_state_init(gateway_state_t *state, uint32_t now_ms)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->last_seen_ms = now_ms;
    state->last_heartbeat_ms = now_ms;
}

gw_sequence_result_t gateway_state_on_telemetry(gateway_state_t *state,
                                                 uint16_t sequence,
                                                 uint32_t now_ms)
{
    gw_sequence_result_t result = gw_accept_sequence(&state->rx, sequence);
    if (result == GW_SEQUENCE_STALE) return result;
    state->last_seen_ms = now_ms;
    state->has_seen_node = true;
    return result;
}

bool gateway_state_online(const gateway_state_t *state, uint32_t now_ms)
{
    return state != NULL && state->has_seen_node &&
           (uint32_t)(now_ms - state->last_seen_ms) < GW_OFFLINE_TIMEOUT_MS;
}

bool gateway_state_offline_due(const gateway_state_t *state, uint32_t now_ms)
{
    return state != NULL && !gateway_state_online(state, now_ms) &&
           (uint32_t)(now_ms - state->last_seen_ms) >= GW_OFFLINE_TIMEOUT_MS;
}

bool gateway_state_heartbeat_due(const gateway_state_t *state, uint32_t now_ms)
{
    return state != NULL &&
           (uint32_t)(now_ms - state->last_heartbeat_ms) >= GW_HEARTBEAT_PERIOD_MS;
}

uint16_t gateway_state_begin_heartbeat(gateway_state_t *state, uint32_t now_ms)
{
    state->last_heartbeat_ms = now_ms;
    state->heartbeat_sequence = ++state->control_sequence;
    state->heartbeat_waiting = true;
    return state->heartbeat_sequence;
}

bool gateway_state_accept_heartbeat(gateway_state_t *state, uint16_t sequence,
                                    uint32_t now_ms)
{
    if (state == NULL || !state->heartbeat_waiting ||
        sequence != state->heartbeat_sequence) return false;
    state->heartbeat_waiting = false;
    state->last_seen_ms = now_ms;
    state->has_seen_node = true;
    return true;
}

uint16_t gateway_state_queue_config(gateway_state_t *state)
{
    state->config_sequence = ++state->control_sequence;
    state->config_pending = true;
    return state->config_sequence;
}

bool gateway_state_config_pending(const gateway_state_t *state,
                                  uint16_t *sequence)
{
    if (state == NULL || !state->config_pending) return false;
    if (sequence != NULL) *sequence = state->config_sequence;
    return true;
}

bool gateway_state_accept_config_ack(gateway_state_t *state,
                                     uint16_t sequence)
{
    if (state == NULL || !state->config_pending ||
        sequence != state->config_sequence) return false;
    state->config_pending = false;
    return true;
}
