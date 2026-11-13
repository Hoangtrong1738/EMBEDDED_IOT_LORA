#include <assert.h>

#include "node_state.h"

int main(void)
{
    node_state_t state;
    node_snapshot_t sample;
    node_limits_t limits = {250U, 10000U, 2000000U, 60, 90U};

    node_state_init(&state);
    node_state_snapshot(&state, &sample);
    assert(sample.voltage_v == 0U && sample.current_ma == 0U);
    assert(sample.power_mw == 0U && sample.energy_wh == 0U);
    assert((sample.status & ST_DHT_FAIL) != 0U);
    assert((sample.status & ST_PZEM_FAIL) != 0U);

    node_state_update_dht(&state, 1U, 27, 65U);
    assert(node_state_evaluate(&state) == 1U);
    node_state_snapshot(&state, &sample);
    assert(sample.temperature_c == 27 && sample.humidity_percent == 65U);
    assert((sample.status & ST_DHT_FAIL) == 0U);
    assert(node_state_evaluate(&state) == 0U);

    node_state_update_dht(&state, 0U, 99, 99U);
    assert(node_state_evaluate(&state) == 1U);
    node_state_snapshot(&state, &sample);
    assert(sample.temperature_c == 27 && sample.humidity_percent == 65U);
    assert((sample.status & ST_DHT_FAIL) != 0U);

    node_state_apply_limits(&state, &limits);
    node_state_update_dht(&state, 1U, 61, 91U);
    node_state_update_power(&state, 251U, 11000U, 2100000U, 12U, 1U);
    assert(node_state_evaluate(&state) == 1U);
    node_state_snapshot(&state, &sample);
    assert((sample.status & ST_OVER_VOLTAGE) != 0U);
    assert((sample.status & ST_OVER_CURRENT) != 0U);
    assert((sample.status & ST_OVER_POWER) != 0U);
    assert((sample.status & ST_OVER_TEMP) != 0U);
    assert((sample.status & ST_OVER_HUMIDITY) != 0U);
    assert((sample.status & ST_PZEM_FAIL) == 0U);
    assert(node_state_evaluate(&state) == 0U);

    node_state_update_power(&state, 0U, 0U, 0U, 0U, 0U);
    node_state_update_dht(&state, 1U, 25, 50U);
    assert(node_state_evaluate(&state) == 1U);
    node_state_snapshot(&state, &sample);
    assert(sample.status == ST_PZEM_FAIL);
    assert(node_state_evaluate(&state) == 0U);
    return 0;
}
