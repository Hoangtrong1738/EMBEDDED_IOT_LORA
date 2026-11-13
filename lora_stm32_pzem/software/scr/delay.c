#include "delay.h"
#include "main.h"

static u32 cyc_per_us;
void delay_init(void)
{
    cyc_per_us = SystemCoreClock / 1000000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable DWT
    DWT->CYCCNT = 0; // Reset the counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // Enable the counter
}

void delay_us(u32 us)
{
    u32 start = DWT->CYCCNT;
    u32 delay_ticks = us * cyc_per_us;
    while ((DWT->CYCCNT - start) < delay_ticks);
}

void delay_ms(u32 ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}