#ifndef NODE_LORA_H
#define NODE_LORA_H
#include "types.h"
#define USE_IWDG 1
#define ACK_RETRIES 3
uint8_t node_init(void);
void node_lora_dio0_isr(void);
#endif /* NODE_LORA_H */
