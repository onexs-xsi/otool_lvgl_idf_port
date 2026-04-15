idf_component_register(SRCS "src/otool_lvgl_port.cpp"
                            "src/otool_lvgl_port_disp.cpp"
                            "src/otool_lvgl_port_touch.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_lcd esp_driver_ppa esp_mm lvgl esp_timer esp_lcd_touch
                    PRIV_REQUIRES log)
