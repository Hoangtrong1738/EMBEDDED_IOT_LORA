#ifndef NODE_BOARD_CONFIG_H_
#define NODE_BOARD_CONFIG_H_

#include "stm32f1xx_hal.h"

/* Product LEDs are wired to GND through their LED/resistor path, so a high
 * GPIO level turns them on. */
#define NODE_LED_SYSTEM_PORT      GPIOC
#define NODE_LED_SYSTEM_PIN       GPIO_PIN_15
#define NODE_LED_LINK_PORT        GPIOA
#define NODE_LED_LINK_PIN         GPIO_PIN_0
#define NODE_LED_ALARM_PORT       GPIOA
#define NODE_LED_ALARM_PIN        GPIO_PIN_1
#define NODE_LED_ACTIVE_STATE     GPIO_PIN_SET
#define NODE_LED_IDLE_STATE       GPIO_PIN_RESET

/* The product buzzer pin was not supplied. Set this to 1 only after wiring
 * NODE_BUZZER_PIN; PA8 is the suggested free output on this board. */
#define NODE_BUZZER_ENABLED       0
#define NODE_BUZZER_PORT          GPIOA
#define NODE_BUZZER_PIN           GPIO_PIN_8
#define NODE_BUZZER_ACTIVE_STATE  GPIO_PIN_SET
#define NODE_BUZZER_IDLE_STATE    GPIO_PIN_RESET

#endif
