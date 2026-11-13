#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gw_limits.h"

#define GW_PACKET_MAGIC            0xA5U
#define GW_NODE_ID                 1U
#define GW_TELEMETRY_PACKET_SIZE   25U
#define GW_MSG_CONFIG              0xC1U
#define GW_MSG_CONFIG_ACK          0xC2U
#define GW_MSG_HEARTBEAT           0xC3U
#define GW_MSG_HEARTBEAT_ACK       0xC4U
#define GW_CONFIG_FRAME_SIZE       21U
#define GW_CONTROL_REPLY_SIZE       8U
#define GW_HEARTBEAT_FRAME_SIZE     7U
#define GW_REPLY_OK                  0U

#define GW_ALERT_VOLTAGE      (1U << 0)
#define GW_ALERT_CURRENT      (1U << 1)
#define GW_ALERT_POWER        (1U << 2)
#define GW_ALERT_TEMPERATURE  (1U << 3)
#define GW_ALERT_HUMIDITY     (1U << 4)

typedef struct {
    uint8_t node_id;
    uint16_t seq;
    uint16_t voltage_v;
    uint32_t current_ma;
    uint32_t power_mw;
    uint32_t energy_wh;
    int16_t temperature_c;
    uint16_t humidity_percent;
    uint8_t status;
} gw_telemetry_t;

typedef struct {
    bool has_sequence;
    uint16_t last_sequence;
    uint32_t lost_packets;
} gw_rx_state_t;

typedef enum {
    GW_SEQUENCE_NEW,
    GW_SEQUENCE_DUPLICATE,
    GW_SEQUENCE_STALE,
} gw_sequence_result_t;

typedef struct {
    uint16_t seq;
    uint8_t status;
} gw_control_ack_t;

typedef struct {
    uint16_t seq;
} gw_heartbeat_t;

bool gw_decode_packet(const uint8_t *packet, size_t length, gw_telemetry_t *out);
uint8_t gw_alert_mask(const gw_telemetry_t *telemetry);
uint8_t gw_alert_mask_with_limits(const gw_telemetry_t *telemetry, const gw_limits_t *limits);
gw_sequence_result_t gw_accept_sequence(gw_rx_state_t *state, uint16_t sequence);
size_t gw_encode_config(uint8_t *frame, size_t capacity, uint16_t sequence,
                        const gw_limits_t *limits);
size_t gw_encode_heartbeat(uint8_t *frame, size_t capacity, uint8_t type,
                           uint16_t sequence);
bool gw_decode_control_ack(const uint8_t *frame, size_t length,
                           uint8_t expected_type, gw_control_ack_t *out);
bool gw_decode_heartbeat(const uint8_t *frame, size_t length,
                         uint8_t expected_type, gw_heartbeat_t *out);
