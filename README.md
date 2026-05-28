# HydroESPro

## **Sistem avansat de control hidroponic bazat pe ESP32**

Un proiect modular, robust și extensibil pentru automatizarea sistemelor hidroponice, aquaponice sau grow tent.

## Caracteristici Principale

- **Arhitectură modulară** (Event Bus + State Machine)
- **Control precis** al pompelor, dozatoare și aeratoare
- **Monitorizare senzori**: pH, EC, temperatură apă/aer, umiditate, presiune, nivel
- **RTC DS3231** cu backup baterie
- **Interfață web** completă + WebSocket
- **WiFi inteligent**: AP + STA cu fallback
- **LCD 2.8" ILI9341** cu LVGL (planificat)
- **2× PCF8574** (expansiune I/O)
- **SD Card** pentru logging și configurări
- **Suport OTA**

## Structura Proiectului

```text
HydroESPro/
├── main/
│   ├── main.c
│   ├── pins.h
│   ├── config.h
│   ├── event_bus/
│   ├── wifi_manager/
│   ├── rtc_manager/
│   ├── time_manager/
│   ├── pcf_manager/
│   ├── i2c_manager/
│   ├── storage_manager/
│   ├── state_manager/
│   └── hydro_manager/
├── components/
├── CMakeLists.txt
├── sdkconfig.defaults
└── README.md
```

## Hardware

- **ESP32 DevKit** (sau custom board)
- **LCD 2.8" ILI9341** (SPI)
- **DS3231 RTC**
- **2× PCF8574** (I2C)
- **AHT20 + BMP280**
- **DS18B20** (temperatură apă)
- **SD Card** (SDMMC)
- **Relee** 5V (via PCF8574)

## Cum se compilează

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py flash monitor
```

## Roadmap

- **Structură modulară + Event Bus**
- **WiFi Manager (AP/STA + fallback)**
- **RTC + Time Management**
- **PCF8574 cu control alimentare**
- **Storage Manager (NVS + LittleFS)**
- **Web Interface completă**
- **Hydroponic Logic (pH/EC control)**
- **LVGL UI pe LCD**

### Dezvoltat cu ❤️ pentru comunitatea hidroponică.

### 2. `CMakeLists.txt` (Top-level)

```cmake
# CMakeLists.txt - HydroESPro
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# =============================================
#               Project Info
# =============================================
project(HydroESPro)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================
#               Componente
# =============================================
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)

# =============================================
#               Configurații globale
# =============================================
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-variable" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-function" APPEND)

message(STATUS "HydroESPro build started - Version 1.0.0")
```
