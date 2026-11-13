idf_component_register(SRCS "main.c" "gateway_protocol.c" "gateway_state.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES gw_settings lora log gateway_alarm esp_driver_uart)
