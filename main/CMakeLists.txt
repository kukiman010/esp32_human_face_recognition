set(src_dirs        ./)

set(include_dirs    ./)

set(requires    human_face_recognition
                spiflash_fatfs
                esp_wifi
                esp_netif
                esp_http_server
                mdns
                nvs_flash
                esp_event)

if (IDF_TARGET STREQUAL "esp32s3")
    list(APPEND requires esp32_s3_eye_noglib
                         esp32-camera
                         led_strip
                         esp_lcd)
elseif (IDF_TARGET STREQUAL "esp32p4")
    list(APPEND requires esp32_p4_function_ev_board_noglib
                         esp_lcd)
endif()

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
