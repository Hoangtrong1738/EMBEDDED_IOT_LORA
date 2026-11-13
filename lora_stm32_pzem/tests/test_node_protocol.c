#include <assert.h>
#include <string.h>

#include "lora_data.h"

int main(void)
{
    node_config_cmd_t config = {0};
    node_heartbeat_t heartbeat = {0};
    u8 frame[32] = {0};
    u8 length;

    frame[0] = PKT_MAGIC;
    frame[1] = MSG_NODE_CONFIG;
    frame[2] = Node_ID;
    frame[3] = 0x34U;
    frame[4] = 0x12U;
    frame[5] = 250U;
    frame[6] = 0U;
    frame[7] = 0x10U; frame[8] = 0x27U; /* 10000 mA */
    frame[11] = 0x80U; frame[12] = 0x84U; frame[13] = 0x1EU; /* 2000000 mW */
    frame[15] = 60U;
    frame[17] = 90U;
    {
        const u16 crc = crc16_modbus(frame, NODE_CONFIG_FRAME_SIZE - 2U);
        frame[NODE_CONFIG_FRAME_SIZE - 2U] = (u8)crc;
        frame[NODE_CONFIG_FRAME_SIZE - 1U] = (u8)(crc >> 8U);
    }

    assert(lora_decode_config(frame, NODE_CONFIG_FRAME_SIZE, &config));
    assert(config.seq == 0x1234U);
    assert(config.voltage_limit == 250U);
    assert(config.current_limit_ma == 10000U);
    assert(config.power_limit_mw == 2000000U);
    assert(config.temp_limit == 60);
    assert(config.hum_limit == 90U);

    frame[NODE_CONFIG_FRAME_SIZE - 1U] ^= 1U;
    assert(!lora_decode_config(frame, NODE_CONFIG_FRAME_SIZE, &config));
    frame[NODE_CONFIG_FRAME_SIZE - 1U] ^= 1U;
    frame[2] = 2U;
    assert(!lora_decode_config(frame, NODE_CONFIG_FRAME_SIZE, &config));

    length = lora_encode_control_reply(frame, sizeof(frame), MSG_NODE_CONFIG_ACK,
                                       0x1234U, NODE_REPLY_OK);
    assert(length == NODE_CONTROL_REPLY_SIZE);
    assert(frame[0] == PKT_MAGIC && frame[1] == MSG_NODE_CONFIG_ACK);

    length = lora_encode_heartbeat(frame, sizeof(frame), MSG_NODE_HEARTBEAT_ACK,
                                   0x4321U);
    assert(length == NODE_HEARTBEAT_FRAME_SIZE);
    assert(lora_decode_heartbeat(frame, length, MSG_NODE_HEARTBEAT_ACK, &heartbeat));
    assert(heartbeat.seq == 0x4321U);
    return 0;
}
