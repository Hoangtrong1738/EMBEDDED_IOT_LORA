#include "gateway_protocol.h"

#define GW_CRC_OFFSET (GW_TELEMETRY_PACKET_SIZE - 2U)


static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static uint16_t crc16_modbus(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    while (length-- > 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U)
                             : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

static void write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static bool valid_control_frame(const uint8_t *frame, size_t length,
                                size_t expected_length, uint8_t expected_type)
{
    return frame != NULL && length == expected_length &&
           frame[0] == GW_PACKET_MAGIC && frame[1] == expected_type &&
           frame[2] == GW_NODE_ID &&
           read_u16_le(&frame[length - 2U]) == crc16_modbus(frame, length - 2U);
}

bool gw_decode_packet(const uint8_t *packet, size_t length, gw_telemetry_t *out)
{
    if (packet == NULL || out == NULL || length != GW_TELEMETRY_PACKET_SIZE ||
        packet[0] != GW_PACKET_MAGIC || packet[1] != GW_NODE_ID ||
        read_u16_le(&packet[GW_CRC_OFFSET]) != crc16_modbus(packet, GW_CRC_OFFSET)) {
        return false;
    }

    out->node_id = packet[1];
    out->seq = read_u16_le(&packet[2]);
    out->voltage_v = read_u16_le(&packet[4]);
    out->current_ma = read_u32_le(&packet[6]);
    out->power_mw = read_u32_le(&packet[10]);
    out->energy_wh = read_u32_le(&packet[14]);
    out->temperature_c = (int16_t)read_u16_le(&packet[18]);
    out->humidity_percent = read_u16_le(&packet[20]);
    out->status = packet[22];
    return true;
}

uint8_t gw_alert_mask(const gw_telemetry_t *telemetry)
{
    gw_limits_t limits;
    gw_limits_defaults(&limits);
    return gw_alert_mask_with_limits(telemetry, &limits);
}

uint8_t gw_alert_mask_with_limits(const gw_telemetry_t *telemetry, const gw_limits_t *limits)
{
    uint8_t alerts = 0U;

    if (telemetry == NULL || limits == NULL) return 0U;
    if (telemetry->voltage_v >= limits->volt_max) alerts |= GW_ALERT_VOLTAGE;
    if (telemetry->current_ma >= (uint32_t)limits->amp_max_x10 * 100U) alerts |= GW_ALERT_CURRENT;
    if (telemetry->power_mw >= (uint32_t)limits->watt_max * 1000U) alerts |= GW_ALERT_POWER;
    if (telemetry->temperature_c >= (int16_t)limits->temp_max) alerts |= GW_ALERT_TEMPERATURE;
    if (telemetry->humidity_percent >= limits->hum_max) alerts |= GW_ALERT_HUMIDITY;
    return alerts;
}

gw_sequence_result_t gw_accept_sequence(gw_rx_state_t *state, uint16_t sequence)
{
    if (state->has_sequence) {
        const uint16_t distance = (uint16_t)(sequence - state->last_sequence);
        if (distance == 0U) return GW_SEQUENCE_DUPLICATE;
        /* Modular ordering is unambiguous for forward distances below half-range. */
        if (distance >= 0x8000U) return GW_SEQUENCE_STALE;
        if (distance > 1U) {
            state->lost_packets += (uint16_t)(distance - 1U);
        }
    }
    state->last_sequence = sequence;
    state->has_sequence = true;
    return GW_SEQUENCE_NEW;
}

size_t gw_encode_config(uint8_t *frame, size_t capacity, uint16_t sequence,
                        const gw_limits_t *limits)
{
    uint16_t crc;
    if (frame == NULL || limits == NULL || capacity < GW_CONFIG_FRAME_SIZE) return 0U;
    frame[0] = GW_PACKET_MAGIC;
    frame[1] = GW_MSG_CONFIG;
    frame[2] = GW_NODE_ID;
    write_u16_le(&frame[3], sequence);
    write_u16_le(&frame[5], limits->volt_max);
    write_u32_le(&frame[7], (uint32_t)limits->amp_max_x10 * 100U);
    write_u32_le(&frame[11], (uint32_t)limits->watt_max * 1000U);
    write_u16_le(&frame[15], limits->temp_max);
    write_u16_le(&frame[17], limits->hum_max);
    crc = crc16_modbus(frame, GW_CONFIG_FRAME_SIZE - 2U);
    write_u16_le(&frame[19], crc);
    return GW_CONFIG_FRAME_SIZE;
}

size_t gw_encode_heartbeat(uint8_t *frame, size_t capacity, uint8_t type,
                           uint16_t sequence)
{
    uint16_t crc;
    if (frame == NULL || capacity < GW_HEARTBEAT_FRAME_SIZE ||
        (type != GW_MSG_HEARTBEAT && type != GW_MSG_HEARTBEAT_ACK)) return 0U;
    frame[0] = GW_PACKET_MAGIC;
    frame[1] = type;
    frame[2] = GW_NODE_ID;
    write_u16_le(&frame[3], sequence);
    crc = crc16_modbus(frame, GW_HEARTBEAT_FRAME_SIZE - 2U);
    write_u16_le(&frame[5], crc);
    return GW_HEARTBEAT_FRAME_SIZE;
}

bool gw_decode_control_ack(const uint8_t *frame, size_t length,
                           uint8_t expected_type, gw_control_ack_t *out)
{
    if (out == NULL || expected_type != GW_MSG_CONFIG_ACK ||
        !valid_control_frame(frame, length, GW_CONTROL_REPLY_SIZE, expected_type)) return false;
    out->seq = read_u16_le(&frame[3]);
    out->status = frame[5];
    return true;
}

bool gw_decode_heartbeat(const uint8_t *frame, size_t length,
                         uint8_t expected_type, gw_heartbeat_t *out)
{
    if (out == NULL ||
        !valid_control_frame(frame, length, GW_HEARTBEAT_FRAME_SIZE,
                             expected_type)) return false;
    out->seq = read_u16_le(&frame[3]);
    return true;
}
