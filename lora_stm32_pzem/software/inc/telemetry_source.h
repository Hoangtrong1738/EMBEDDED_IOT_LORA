#ifndef TELEMETRY_SOURCE_H
#define TELEMETRY_SOURCE_H

#include <stdint.h>

typedef struct {
    uint16_t voltage_v;
    uint32_t current_ma;
    uint32_t power_mw;
    uint32_t energy_wh;
    int16_t temperature_c;
    uint16_t humidity_percent;
    uint8_t status;
} telemetry_sample_t;

void telemetry_source_init(void);
void telemetry_source_next(telemetry_sample_t *sample);

#endif
