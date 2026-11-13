#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gateway_protocol.h"

#define GW_HEARTBEAT_PERIOD_MS 5000U
#define GW_OFFLINE_TIMEOUT_MS 15000U

typedef struct {
    gw_rx_state_t rx;
    uint32_t last_seen_ms;
    uint32_t last_heartbeat_ms;
    uint16_t control_sequence;
    uint16_t heartbeat_sequence;
    uint16_t config_sequence;
    bool has_seen_node;
    bool heartbeat_waiting;
    bool config_pending;
} gateway_state_t;

void gateway_state_init(gateway_state_t *state, uint32_t now_ms);
gw_sequence_result_t gateway_state_on_telemetry(gateway_state_t *state,
                                                 uint16_t sequence,
                                                 uint32_t now_ms);
bool gateway_state_online(const gateway_state_t *state, uint32_t now_ms);
bool gateway_state_offline_due(const gateway_state_t *state, uint32_t now_ms);
bool gateway_state_heartbeat_due(const gateway_state_t *state, uint32_t now_ms);
uint16_t gateway_state_begin_heartbeat(gateway_state_t *state, uint32_t now_ms);
bool gateway_state_accept_heartbeat(gateway_state_t *state, uint16_t sequence,
                                    uint32_t now_ms);
uint16_t gateway_state_queue_config(gateway_state_t *state);
bool gateway_state_config_pending(const gateway_state_t *state,
                                  uint16_t *sequence);
bool gateway_state_accept_config_ack(gateway_state_t *state,
                                     uint16_t sequence);
