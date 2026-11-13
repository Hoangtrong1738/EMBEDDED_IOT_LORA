#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include <stdint.h>

typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

typedef struct GPIO_TypeDef GPIO_TypeDef;
struct GPIO_TypeDef { uint32_t unused; };
typedef uint32_t GPIO_PinState;

extern GPIO_TypeDef test_gpio_b;
#define GPIOB (&test_gpio_b)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_MODE_OUTPUT_OD 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, uint32_t state);
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);

#endif
