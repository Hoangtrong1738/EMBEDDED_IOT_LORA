/*
 * lora_data.h
 *
 *  Created on: Oct 26, 2026
 *      Author: Admin
 */

#ifndef SRC_LORA_DATA_H_
#define SRC_LORA_DATA_H_
#include "stdint.h"
#include "types.h"
/*Dinh danh*/
#ifndef Node_ID 
#define Node_ID 1
#endif	

#define PKT_MAGIC 0xA5
#define MSG_NODE_CONFIG       0xC1 /* Ma lenh gateway gui nguong xuong node. */
#define MSG_NODE_CONFIG_ACK   0xC2 /* Ma phan hoi node xac nhan lenh cau hinh. */
#define MSG_NODE_HEARTBEAT    0xC3
#define MSG_NODE_HEARTBEAT_ACK 0xC4

#define NODE_CONFIG_FRAME_SIZE    21U
#define NODE_CONTROL_REPLY_SIZE    8U
#define NODE_HEARTBEAT_FRAME_SIZE  7U
#define NODE_REPLY_OK               0U
#define NODE_REPLY_INVALID          1U

/*Bit trang thai */
#define ST_OVER_CURRENT 	(1 << 0) // qua dong
#define ST_OVER_TEMP 		(1 << 1) // qua nhiet do
#define ST_PZEM_FAIL 	    (1 << 2) // PZEM khong hoat dong
#define ST_WDG_RESET 	    (1 << 3) // NODE vua reset do WDG
#define ST_DHT_FAIL 	    (1 << 4) // loi cam bien DHT11
#define ST_OVER_VOLTAGE    (1 << 5)
#define ST_OVER_POWER      (1 << 6)
#define ST_OVER_HUMIDITY   (1 << 7)
/* -------------------------------------------------------------------------- */
/*IO port and pin of LoRa*/

#define LORA_NSS_PORT GPIOA
#define LORA_NSS_PIN GPIO_PIN_4
#define LORA_RST_PORT GPIOB
#define LORA_RST_PIN GPIO_PIN_0
#define DIO0_PORT GPIOB
#define DIO0_PIN GPIO_PIN_1
#define DIO1_PORT GPIOB
#define DIO1_PIN GPIO_PIN_10

typedef struct __attribute__((packed))
{
	u8 magic;
	u8 node_id;
	u16 seq;
	u16 volt;
	u32 mA;
	u32 pwr;
	u32 Wh;
	i16 temp;
	u16	hum;
	u8 	status;
	u16 crc16;

}lora_data_pkt_t;

typedef struct __attribute__((packed))
{
	u8 magic;
	u8 node_id;
	u16	seq;
}ack_t;

/*gói Lệnh Gateway -> node để đổi ngưỡng cảnh báo*/
typedef struct __attribute__((packed))
{
	u8 magic;
	u8 type;
	u8 node_id;
	u16	seq;
	u16 voltage_limit;
	u32 current_limit_ma;
	u32 power_limit_mw;
	i16 temp_limit;
	u16 hum_limit;
	u16 crc16;
}node_config_cmd_t;

/*ACK cấu hình node -> gateway*/
typedef struct __attribute__((packed))
{
	u8 magic;
	u8 type;
	u8 node_id;
	u16	seq;
	u8 status; // 0: OK, 1: lỗi
	u16 crc16;
}node_config_ack_t;

typedef struct __attribute__((packed))
{
	u8 magic;
	u8 type;
	u8 node_id;
	u16 seq;
	u16 crc16;
} node_heartbeat_t;

u16 crc16_modbus(const u8 *d, u8 len);
u8 lora_decode_config(const u8 *frame, u8 length, node_config_cmd_t *config);
u8 lora_decode_heartbeat(const u8 *frame, u8 length, u8 expected_type,
                         node_heartbeat_t *heartbeat);
u8 lora_encode_control_reply(u8 *frame, u8 capacity, u8 type, u16 seq,
                             u8 status);
u8 lora_encode_heartbeat(u8 *frame, u8 capacity, u8 type, u16 seq);
u8 node_init(void);
#endif /* SRC_LORA_DATA_H_ */
