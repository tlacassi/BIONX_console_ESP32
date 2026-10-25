idf_component_register(SRCS "can_observer.c" INCLUDE_DIRS "."
    REQUIRES display mcp2515 bionx_protocol esp_lvgl_port esp_timer)
