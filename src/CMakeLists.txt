idf_component_register(
    SRCS
        "main.c"
        "app_state.c"
        "gpio_hw.c"
        "light_pwm.c"
        "effects.c"
        "wifi_ap.c"
        "web_server.c"
        "index.html.S"
    INCLUDE_DIRS "."
)

target_link_libraries(${COMPONENT_LIB} PRIVATE m)
