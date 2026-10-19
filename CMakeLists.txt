idf_component_register(SRCS "src/otool_lvgl_port.cpp"
                            "src/otool_lvgl_metrics.cpp"
                            "src/otool_draw_probe.cpp"
                            "src/otool_present_fence.cpp"
                            "src/otool_lvgl_port_disp.cpp"
                            "src/otool_lvgl_port_touch.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_lcd esp_driver_ppa esp_mm lvgl esp_timer esp_lcd_touch
                    PRIV_REQUIRES log esp_driver_dma)

foreach(primitive fill border line label letter image)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=lv_draw_sw_${primitive}")
endforeach()
if(IDF_TARGET STREQUAL "esp32p4")
    target_link_options(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=dw_gdma_lli_config_transfer" "-Wl,--wrap=dw_gdma_channel_use_link_list")
endif()
