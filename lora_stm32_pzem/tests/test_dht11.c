#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "dht11.h"
#include "main.h"

GPIO_TypeDef test_gpio_b;

static uint8_t waveform[1024];
static size_t waveform_length;
static size_t waveform_index;

static void append_level(uint8_t level, size_t reads)
{
    while (reads-- > 0U) {
        waveform[waveform_length++] = level;
    }
}

static void load_frame(uint8_t humidity, uint8_t temperature, uint8_t checksum)
{
    uint8_t bytes[5] = {humidity, 0U, temperature, 0U, checksum};
    size_t byte_index;
    int bit;

    waveform_length = 0U;
    waveform_index = 0U;
    /* DHT11 acknowledgement: 80 us low, then 80 us high. */
    append_level(GPIO_PIN_RESET, 4U);
    append_level(GPIO_PIN_SET, 4U);

    for (byte_index = 0U; byte_index < 5U; byte_index++) {
        for (bit = 7; bit >= 0; bit--) {
            append_level(GPIO_PIN_RESET, 3U);
            append_level(GPIO_PIN_SET, (bytes[byte_index] & (1U << bit)) ? 8U : 3U);
        }
    }
    append_level(GPIO_PIN_RESET, 3U);
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
    (void)port;
    (void)init;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, uint32_t state)
{
    (void)port;
    (void)pin;
    (void)state;
}

uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    if (waveform_index >= waveform_length) {
        return GPIO_PIN_SET;
    }
    return waveform[waveform_index++];
}

void delay_us(uint32_t us) { (void)us; }
void delay_ms(uint32_t ms) { (void)ms; }

int main(void)
{
    dht11_data_t measurement;

    load_frame(65U, 27U, 92U);
    assert(dht11_read(&measurement) == 0);
    assert(measurement.hum == 65U);
    assert(measurement.temp == 27U);

    load_frame(65U, 27U, 0U);
    assert(dht11_read(&measurement) != 0);
    return 0;
}
