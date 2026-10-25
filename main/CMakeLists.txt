idf_component_register(
    SRCS "main.c" "board_config.c" "bionx_diagnostic.c" "bionx_master_app.c" "bench_app.c" "jog_app.c" "component_test.c" "screen_test.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer display dashboard mcp2515 bionx_protocol can_observer bionx_master esp_lvgl_port
)
