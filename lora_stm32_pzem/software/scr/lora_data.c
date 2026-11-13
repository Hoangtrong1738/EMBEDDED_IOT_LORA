/*
 * lora_data.c
 *
 *  Created on: Oct 26, 2026
 *      Author: Admin
 */

#include "lora_data.h"

_Static_assert(sizeof(lora_data_pkt_t) == 25U, "telemetry wire size");
_Static_assert(sizeof(ack_t) == 4U, "telemetry ACK wire size");
_Static_assert(sizeof(node_config_cmd_t) == NODE_CONFIG_FRAME_SIZE,
               "config wire size");
_Static_assert(sizeof(node_config_ack_t) == NODE_CONTROL_REPLY_SIZE,
               "config ACK wire size");
_Static_assert(sizeof(node_heartbeat_t) == NODE_HEARTBEAT_FRAME_SIZE,
               "heartbeat wire size");

static u16 read_u16_le(const u8 *data)
{
	return (u16)data[0] | ((u16)data[1] << 8U);
}

static u32 read_u32_le(const u8 *data)
{
	return (u32)data[0] | ((u32)data[1] << 8U) |
	       ((u32)data[2] << 16U) | ((u32)data[3] << 24U);
}

static void write_u16_le(u8 *data, u16 value)
{
	data[0] = (u8)value;
	data[1] = (u8)(value >> 8U);
}


u16 crc16_modbus(const u8 *d, u8 len)
{
	u16 crc = 0xFFFF;
	while(len--)
	{
		crc ^= *d++;
		for(u8 i=0;i<8;i++)
		{
			crc =(crc&1) ? (crc>>1) ^ 0xA001 : (crc >> 1);
		}
	}
	return crc;
}

static u8 valid_control_frame(const u8 *frame, u8 length, u8 expected_length,
						      u8 expected_type)
{
	u16 received_crc;

	if (frame == 0 || length != expected_length || frame[0] != PKT_MAGIC ||
		frame[1] != expected_type || frame[2] != Node_ID) {
		return 0U;
	}
	received_crc = read_u16_le(&frame[length - 2U]);
	return (u8)(received_crc == crc16_modbus(frame, (u8)(length - 2U)));
}

u8 lora_decode_config(const u8 *frame, u8 length, node_config_cmd_t *config)
{
	if (config == 0 ||
		!valid_control_frame(frame, length, NODE_CONFIG_FRAME_SIZE,
						 MSG_NODE_CONFIG)) {
		return 0U;
	}

	config->magic = frame[0];
	config->type = frame[1];
	config->node_id = frame[2];
	config->seq = read_u16_le(&frame[3]);
	config->voltage_limit = read_u16_le(&frame[5]);
	config->current_limit_ma = read_u32_le(&frame[7]);
	config->power_limit_mw = read_u32_le(&frame[11]);
	config->temp_limit = (i16)read_u16_le(&frame[15]);
	config->hum_limit = read_u16_le(&frame[17]);
	config->crc16 = read_u16_le(&frame[19]);
	return 1U;
}

u8 lora_decode_heartbeat(const u8 *frame, u8 length, u8 expected_type,
						 node_heartbeat_t *heartbeat)
{
	if (heartbeat == 0 ||
		!valid_control_frame(frame, length, NODE_HEARTBEAT_FRAME_SIZE,
						 expected_type)) {
		return 0U;
	}
	heartbeat->magic = frame[0];
	heartbeat->type = frame[1];
	heartbeat->node_id = frame[2];
	heartbeat->seq = read_u16_le(&frame[3]);
	heartbeat->crc16 = read_u16_le(&frame[5]);
	return 1U;
}

u8 lora_encode_control_reply(u8 *frame, u8 capacity, u8 type, u16 seq,
							 u8 status)
{
	u16 crc;
	if (frame == 0 || capacity < NODE_CONTROL_REPLY_SIZE ||
		(type != MSG_NODE_CONFIG_ACK)) {
		return 0U;
	}
	frame[0] = PKT_MAGIC;
	frame[1] = type;
	frame[2] = Node_ID;
	write_u16_le(&frame[3], seq);
	frame[5] = status;
	crc = crc16_modbus(frame, NODE_CONTROL_REPLY_SIZE - 2U);
	write_u16_le(&frame[6], crc);
	return NODE_CONTROL_REPLY_SIZE;
}

u8 lora_encode_heartbeat(u8 *frame, u8 capacity, u8 type, u16 seq)
{
	u16 crc;
	if (frame == 0 || capacity < NODE_HEARTBEAT_FRAME_SIZE ||
		(type != MSG_NODE_HEARTBEAT && type != MSG_NODE_HEARTBEAT_ACK)) {
		return 0U;
	}
	frame[0] = PKT_MAGIC;
	frame[1] = type;
	frame[2] = Node_ID;
	write_u16_le(&frame[3], seq);
	crc = crc16_modbus(frame, NODE_HEARTBEAT_FRAME_SIZE - 2U);
	write_u16_le(&frame[5], crc);
	return NODE_HEARTBEAT_FRAME_SIZE;
}





