#include <assert.h>
#include <stdint.h>

#include "dht11.h"

static void make_capture(uint16_t edges[DHT11_CAPTURE_EDGE_COUNT],
                         uint8_t checksum)
{
    const uint8_t bytes[5] = {65U, 0U, 27U, 0U, checksum};
    uint16_t elapsed = 0U;

    edges[0] = elapsed;
    elapsed = (uint16_t)(elapsed + 80U);
    edges[1] = elapsed;
    elapsed = (uint16_t)(elapsed + 50U);

    for (uint8_t bit = 0U; bit < 40U; bit++) {
        const uint8_t rising = (uint8_t)(2U + bit * 2U);
        const uint8_t falling = (uint8_t)(rising + 1U);
        const uint8_t value =
            (uint8_t)((bytes[bit / 8U] >> (7U - bit % 8U)) & 1U);
        edges[rising] = elapsed;
        elapsed = (uint16_t)(elapsed + (value != 0U ? 70U : 28U));
        edges[falling] = elapsed;
        elapsed = (uint16_t)(elapsed + 50U);
    }
}

int main(void)
{
    uint16_t edges[DHT11_CAPTURE_EDGE_COUNT];
    dht11_data_t data;

    make_capture(edges, 92U);
    assert(dht11_decode_capture(edges, DHT11_CAPTURE_EDGE_COUNT, &data) == 0);
    assert(data.hum == 65U);
    assert(data.temp == 27U);
    assert(dht11_decode_capture(edges, DHT11_CAPTURE_EDGE_COUNT - 1U, &data) ==
           -2);

    make_capture(edges, 93U);
    assert(dht11_decode_capture(edges, DHT11_CAPTURE_EDGE_COUNT, &data) == -3);
    assert(dht11_decode_capture(edges, DHT11_CAPTURE_EDGE_COUNT, NULL) == -1);
    return 0;
}
