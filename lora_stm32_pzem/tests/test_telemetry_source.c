#include <assert.h>

#include "../software/inc/telemetry_source.h"

int main(void)
{
    telemetry_sample_t first;
    telemetry_sample_t second;

    telemetry_source_init();
    telemetry_source_next(&first);
    telemetry_source_next(&second);

    assert(first.voltage_v != 0U);
    assert(first.current_ma != 0U);
    assert(first.power_mw != 0U);
    assert(second.energy_wh >= first.energy_wh);
    assert(first.voltage_v != second.voltage_v || first.temperature_c != second.temperature_c);
    return 0;
}
