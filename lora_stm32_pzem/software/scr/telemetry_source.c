#include "telemetry_source.h"

/* Offline test profile: normal values followed by threshold violations. */
static const telemetry_sample_t samples[] = {
    {230U, 1250U, 287500U, 100U, 25, 50U, 0U},
    {231U, 1600U, 369600U, 101U, 27, 55U, 0U},
    {228U, 9000U, 2052000U, 102U, 32, 65U, 0U},
    {251U, 10500U, 2415000U, 103U, 61, 91U, 0U},
    {235U, 2200U, 517000U, 104U, 29, 58U, 0U},
};

static uint8_t sample_index;

void telemetry_source_init(void)
{
    sample_index = 0U;
}

void telemetry_source_next(telemetry_sample_t *sample)
{
    if (sample == 0) {
        return;
    }

    *sample = samples[sample_index];
    sample_index = (uint8_t)((sample_index + 1U) %
                             (sizeof(samples) / sizeof(samples[0])));
}
