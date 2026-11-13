#include "dht11.h"

#define DHT_DATA_BIT_THRESHOLD_US 50U

int dht11_decode_capture(const volatile uint16_t *edges, size_t edge_count,
                         dht11_data_t *data)
{
    uint8_t bytes[5] = {0};

    if (edges == NULL || data == NULL) return -1;
    if (edge_count != DHT11_CAPTURE_EDGE_COUNT) return -2;

    for (uint8_t bit = 0U; bit < 40U; bit++) {
        const uint8_t rising_edge = (uint8_t)(2U + bit * 2U);
        const uint8_t falling_edge = (uint8_t)(rising_edge + 1U);
        const uint16_t high_us =
            (uint16_t)(edges[falling_edge] - edges[rising_edge]);
        if (high_us > DHT_DATA_BIT_THRESHOLD_US) {
            bytes[bit / 8U] |= (uint8_t)(1U << (7U - bit % 8U));
        }
    }

    if ((uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]) != bytes[4]) {
        return -3;
    }

    data->hum = bytes[0];
    data->temp = bytes[2];
    return 0;
}
