#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Definitions
namespace Pins {
    constexpr uint8_t STEPPER_STEP = 2;
    constexpr uint8_t STEPPER_DIR = 3;
    constexpr uint8_t STEPPER_ENABLE = 4;
    constexpr uint8_t START_BUTTON = 5;
    constexpr uint8_t STATUS_LED = 13;  // Built-in LED
    
    // SD Card uses built-in SDIO pins on Teensy 4.1:
    // SDIO_CLK  = 45
    // SDIO_CMD  = 43
    // SDIO_DAT0 = 42
    // SDIO_DAT1 = 44
    // SDIO_DAT2 = 46
    // SDIO_DAT3 = 47
}

// SD Card Configuration
namespace SDConfig {
    constexpr const char* JSON_FILENAME = "commands.json";
    constexpr const char* BINARY_FILENAME = "commands.bin";
}

// FreeRTOS Configuration
namespace RTOSConfig {
    constexpr uint32_t SD_TASK_STACK_SIZE = 4096;
    constexpr uint32_t STEPPER_TASK_STACK_SIZE = 2048;
    constexpr uint32_t BUTTON_TASK_STACK_SIZE = 1024;
    
    constexpr UBaseType_t SD_TASK_PRIORITY = 2;
    constexpr UBaseType_t STEPPER_TASK_PRIORITY = 3;  // Highest priority
    constexpr UBaseType_t BUTTON_TASK_PRIORITY = 2;
}

#endif // CONFIG_H
