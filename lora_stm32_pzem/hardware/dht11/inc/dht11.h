#ifndef DHT11_H
#define DHT11_H

#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define DHT11_CAPTURE_EDGE_COUNT 82U

typedef struct {
    u16 temp;
    u16 hum;
} dht11_data_t;

int dht11_init(void);
int dht11_start(void);
int dht11_capture_start(void);
int dht11_capture_finish(dht11_data_t *data);
void dht11_abort(void);
void dht11_tim2_irq_handler(void);
int dht11_decode_capture(const volatile uint16_t *edges, size_t edge_count,
                         dht11_data_t *data);


#endif /* DHT11_H */
