$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw "$PSScriptRoot/../hardware/lora/inc/LoRa.h"
$source = Get-Content -Raw "$PSScriptRoot/../hardware/lora/src/LoRa.c"

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw "FAIL: $Message"
    }
}

# The public API carries physical dBm values; the driver must derive the
# SX127x PA_BOOST register value as 0x80 | (dBm - 2).
Assert-Contains $header '#define\s+LORA_POWER_17DBM\s+17U' '17 dBm must remain distinguishable from +20 dBm.'
Assert-Contains $header '#define\s+LORA_POWER_20DBM\s+20U' '20 dBm must remain distinguishable from +17 dBm.'
Assert-Contains $source '0x80U\s*\|\s*\(power\s*-\s*2U\)' 'LoRa_setPower must encode PA_BOOST output power from dBm.'

# A frame marked RxDone with PayloadCrcError must never be handed to the ACK parser.
Assert-Contains $source 'irqFlags\s*&\s*0x20U' 'LoRa_receive must reject SX127x PayloadCrcError.'
Assert-Contains $source 'LoRa_write\(_LoRa, REG_IRQ_FLAGS, irqFlags\)' 'LoRa_receive must clear exactly the observed IRQ flags.'

# During ACK polling the radio must remain in RX continuous until RxDone is
# observed; entering standby before checking IRQ can lose the short ACK preamble.
Assert-Contains $source '(?s)/\* Inspect IRQ while RX continuous; do not create an ACK receive gap\. \*/\s*irqFlags = LoRa_read\(_LoRa, REG_IRQ_FLAGS\);\s*if \(irqFlags & 0x40U\) \{.*?LoRa_gotoMode\(_LoRa, LORA_MODE_STANDBY\);' 'LoRa_receive must wait for RxDone before entering standby.'

Write-Output 'PASS: SX127x PA and RX-CRC register contract'
