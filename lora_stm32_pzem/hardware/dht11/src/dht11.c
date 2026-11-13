#include "main.h"
#include "delay.h"
#include "dht11.h"
#include "stm32f1xx_hal_tim.h"

#include "FreeRTOS.h"
#include "task.h"

#define DHT_PORT GPIOB
#define DHT_PIN GPIO_PIN_3
#define DHT_DATA_BIT_THRESHOLD_US 50U

static TIM_HandleTypeDef s_capture_timer;
static volatile uint16_t s_capture_edges[DHT11_CAPTURE_EDGE_COUNT];
static volatile uint8_t s_capture_count;
static volatile uint8_t s_capture_active;
static volatile uint8_t s_capture_done;
static TaskHandle_t s_capture_task;

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

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *timer)
{
    if (timer->Instance != TIM2) return;

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_AFIO_REMAP_TIM2_PARTIAL_1();
    dht11_set_input();

    HAL_NVIC_SetPriority(TIM2_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

int dht11_init(void)
{
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1) {
        timer_clock_hz *= 2U;
    }
    if (timer_clock_hz < 1000000U || timer_clock_hz % 1000000U != 0U) {
        return -1;
    }

    s_capture_timer.Instance = TIM2;
    s_capture_timer.Init.Prescaler = timer_clock_hz / 1000000U - 1U;
    s_capture_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_capture_timer.Init.Period = 0xFFFFU;
    s_capture_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_capture_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_IC_Init(&s_capture_timer) != HAL_OK) return -1;

    TIM_IC_InitTypeDef input_capture = {0};
    input_capture.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    input_capture.ICSelection = TIM_ICSELECTION_DIRECTTI;
    input_capture.ICPrescaler = TIM_ICPSC_DIV1;
    input_capture.ICFilter = 0U;
    if (HAL_TIM_IC_ConfigChannel(&s_capture_timer, &input_capture,
                                 TIM_CHANNEL_2) != HAL_OK) {
        return -1;
    }

    s_capture_count = 0U;
    s_capture_active = 0U;
    s_capture_done = 0U;
    s_capture_task = NULL;
    dht11_set_output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    return 0;
}

int dht11_start(void)
{
    if (s_capture_active != 0U) return -1;
    dht11_set_output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    return 0;
}

int dht11_capture_start(void)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (task == NULL || s_capture_active != 0U) return -1;

    (void)ulTaskNotifyTake(pdTRUE, 0U);
    s_capture_task = task;
    s_capture_count = 0U;
    s_capture_done = 0U;

    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    delay_us(30U);
    dht11_set_input();

    s_capture_timer.Instance->CNT = 0U;
    CLEAR_BIT(s_capture_timer.Instance->CCER, TIM_CCER_CC2P);
    __HAL_TIM_CLEAR_FLAG(&s_capture_timer, TIM_FLAG_CC2);
    s_capture_active = 1U;
    if (HAL_TIM_IC_Start_IT(&s_capture_timer, TIM_CHANNEL_2) != HAL_OK) {
        s_capture_active = 0U;
        s_capture_task = NULL;
        dht11_set_output();
        HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
        return -1;
    }
    return 0;
}

void dht11_abort(void)
{
    s_capture_active = 0U;
    (void)HAL_TIM_IC_Stop_IT(&s_capture_timer, TIM_CHANNEL_2);
    s_capture_task = NULL;
    s_capture_done = 0U;
    dht11_set_output();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
}

int dht11_capture_finish(dht11_data_t *data)
{
    if (data == NULL) {
        dht11_abort();
        return -1;
    }
    if (s_capture_done == 0U || s_capture_count != DHT11_CAPTURE_EDGE_COUNT) {
        dht11_abort();
        return -2;
    }

    const int result = dht11_decode_capture(s_capture_edges, s_capture_count,
                                            data);
    dht11_abort();
    return result;
}

void dht11_tim2_irq_handler(void)
{
    HAL_TIM_IRQHandler(&s_capture_timer);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *timer)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    TaskHandle_t task;

    if (timer->Instance != TIM2 || s_capture_active == 0U) return;

    if (s_capture_count < DHT11_CAPTURE_EDGE_COUNT) {
        s_capture_edges[s_capture_count] =
            (uint16_t)HAL_TIM_ReadCapturedValue(timer, TIM_CHANNEL_2);
        s_capture_count++;
    }

    if (s_capture_count == DHT11_CAPTURE_EDGE_COUNT) {
        s_capture_done = 1U;
        s_capture_active = 0U;
        __HAL_TIM_DISABLE_IT(timer, TIM_IT_CC2);
        __HAL_TIM_DISABLE(timer);
        task = s_capture_task;
        s_capture_task = NULL;
        if (task != NULL) {
            vTaskNotifyGiveFromISR(task, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
        return;
    }

    timer->Instance->CCER ^= TIM_CCER_CC2P;
}
