// May 3rd 2023, Modified by Abdellah ESSETTY,
// The original program was written under open license. 
// This header file is aiming to provide an interface to interract with sd_card via an SPI bus. 
// The file system used is FAT32.
// This is really just a modification of the official example provided by ESP-IDF.
// Some useful links:
// ESP-idf support for FATFS https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html
// Official example for running an sd-card https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html
// Official documentation for FATFS and its API http://elm-chan.org/fsw/ff/00index_e.html

// Since the ESP-idf has some weird naming conventions, and to avoid confusion, custom and wrapper functions will 
// be written in camelCase, whereas variables will be written in snake_case.

// For error handling, for compatibility reasons, global error variables are used, 
// in that sense, functions would expect a pointer to the error_variable as a first argument and then modify it.
#pragma once

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define MOUNT_POINT "/sdcard"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO 19 // CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI 23 // CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK  18 // CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS  5 //  CONFIG_EXAMPLE_PIN_CS

// static const char* TAG = "SD-CARD H";
static const char* TAG_H = "SD_CARD_H";

esp_vfs_fat_sdmmc_mount_config_t mountConfig()
{
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    // Note: DO NOT RELY ON THIS. It is better to flash your sd-card before mounting.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    return mount_config;
}


// Use settings defined above to initialize SD card and mount FAT filesystem.
// Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
// Please check its source code and implement error recovery when developing
// production applications.

sdmmc_host_t initializeSpi(esp_err_t* ret)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    *ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (*ret != ESP_OK) {
        ESP_LOGE(TAG_H, "Failed to initialize bus.");
        // return NULL;
    }
    return host;
}

