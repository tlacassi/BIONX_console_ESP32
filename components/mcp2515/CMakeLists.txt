idf_component_register(SRCS "mcp2515_core.c" "mcp2515_transport.c" INCLUDE_DIRS "."
    REQUIRES bionx_protocol board_spi driver esp_timer)
