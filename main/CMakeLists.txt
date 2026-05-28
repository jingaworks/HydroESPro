idf_component_register(SRCS 
                        
                        "main.c"

                        "event_bus.c"
                        "hydro_manager.c"
                        "i2c_manager.c"
                        "lcd_manager.c"
                        "pcf_manager.c"
                        "rtc_manager.c"
                        "sensor_manager.c" # work on it
                        "state_manager.c"
                        "storage_manager.c"
                        "time_manager.c"
                        "wifi_manager.c"

                        "ds18b20.c"
                        "board.c"
                        "button.c"
                        "buzzer.c"
                        "relays.c"

                        "http_server.c"
                        "keep_alive.c"
                        "utill.c"

                        # "wifi_conn.c"
                        
                    INCLUDE_DIRS "."
                    EMBED_FILES 
                        "favicon.ico"
                        "style.css"
                        "script.js"
                        "index.html")
