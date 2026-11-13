#include "delay.h"
#include "main.h"
#include "dht11.h"

#define DHT_PORT GPIOB
#define DHT_PIN GPIO_PIN_3

#define DHT_TIMEOUT_POLLS  100U

static void dht11_set_output(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DHT_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &gpio);
}

static void dht11_set_input(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DHT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &gpio);
}

static int dht11_wait_for_level(GPIO_PinState level)
{
    u16 polls = DHT_TIMEOUT_POLLS;

    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) != level) {
        if (polls-- == 0U) {
            return -1;
        }
        delay_us(1U);
    }
    return 0;
}

void dht11_init(void)
{
    dht11_set_output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
}

int dht11_read(dht11_data_t *data)
{
    u8 bytes[5] = {0};
    u8 byte_index;
    u8 bit_index;

    if (data == 0) {
        return -1;
    }

    dht11_set_output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    delay_ms(18U);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    delay_us(30U);
    dht11_set_input();

    if (dht11_wait_for_level(GPIO_PIN_RESET) != 0 ||
        dht11_wait_for_level(GPIO_PIN_SET) != 0 ||
        dht11_wait_for_level(GPIO_PIN_RESET) != 0) {
        return -2;
    }

    for (byte_index = 0U; byte_index < 5U; byte_index++) {
        for (bit_index = 0U; bit_index < 8U; bit_index++) {
            if (dht11_wait_for_level(GPIO_PIN_SET) != 0) {
                return -2;
            }
            delay_us(40U);
            if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET) {
                bytes[byte_index] |= (u8)(1U << (7U - bit_index));
            }
            if (dht11_wait_for_level(GPIO_PIN_RESET) != 0) {
                return -2;
            }
        }
    }

    if ((u8)(bytes[0] + bytes[1] + bytes[2] + bytes[3]) != bytes[4]) {
        return -3;
    }

    /* DHT11 fractional bytes are normally zero; the existing telemetry
     * contract stores whole degrees C and whole percent RH. */
    data->hum = bytes[0];
    data->temp = bytes[2];
    return 0;
}

