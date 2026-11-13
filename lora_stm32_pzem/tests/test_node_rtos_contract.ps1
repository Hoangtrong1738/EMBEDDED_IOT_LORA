$ErrorActionPreference = 'Stop'

$node = Get-Content -Raw "$PSScriptRoot/../software/scr/node_lora.c"
$gpio = Get-Content -Raw "$PSScriptRoot/../Core/Src/gpio.c"
$irq = Get-Content -Raw "$PSScriptRoot/../Core/Src/stm32f1xx_it.c"
$board = Get-Content -Raw "$PSScriptRoot/../software/inc/node_board_config.h"

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) { throw "FAIL: $Message" }
}

Assert-Contains $node '"dht"' 'Node must create a DHT task.'
Assert-Contains $node '"power"' 'Node must keep a one-second placeholder power task.'
Assert-Contains $node '"monitor"' 'Node must evaluate alarm transitions in a monitor task.'
Assert-Contains $node '"radio"' 'Node must have one radio-owning task.'
Assert-Contains $node 'vTaskNotifyGiveFromISR' 'DIO0 ISR callback must notify the radio task.'
Assert-Contains $node 'vTaskDelayUntil' 'One-second sensor scheduling must not drift.'
Assert-Contains $gpio 'GPIO_MODE_IT_RISING' 'PB1 DIO0 must use rising-edge EXTI.'
Assert-Contains $irq 'EXTI1_IRQHandler' 'EXTI1 interrupt handler must dispatch PB1.'
Assert-Contains $node 'NODE_TX_PERIOD_MS\s+10000U' 'Periodic telemetry must be ten seconds.'
Assert-Contains $node 'NODE_BUZZER_ENABLED' 'Node alarm handling must support a configurable buzzer output.'
Assert-Contains $board 'NODE_BUZZER_PIN' 'Node board config must expose the buzzer pin.'
Assert-Contains $board 'NODE_LED_SYSTEM_PIN\s+GPIO_PIN_15' 'Node system LED must use PC15.'
Assert-Contains $board 'NODE_LED_LINK_PIN\s+GPIO_PIN_0' 'Node link LED must use PA0.'
Assert-Contains $board 'NODE_LED_ALARM_PIN\s+GPIO_PIN_1' 'Node alarm LED must use PA1.'
Write-Output 'PASS: STM32 Node 1 RTOS event contract'
