#ifndef DHT11_H
#define DHT11_H

#include "types.h"

typedef struct {
    u16 temp;
    u16 hum;
} dht11_data_t;

void dht11_init(void);
/* Returns 0 on success, -1 for an invalid argument, -2 on timeout, and -3
 * when the sensor checksum does not match. */
int dht11_read(dht11_data_t* data);


#endif /* DHT11_H */
