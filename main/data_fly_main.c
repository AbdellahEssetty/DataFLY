/* DATA FLY : Data logger for micro-mobility vehicles.
    This project aim to develop a data logger for small vehicles with diffrent abilities.
    This project is a part of the end-of-study internship of Abdellah ESSETTY, hold at Stellantis\
    between february 1st and August 1st.

    Good luck for anyone who tries to enhace this code, I've tried my best here, Hope you find it easy to navigate.

    here is the structure of the project
    |-build **DO NOT TOUCH THIS.
    |-include 
        |-header_1.h  **Header files, I've tried to minimize code, so there are no source files. 
        |-header_2.h  **when included, there is no need to add relative path. e.g in main just do #include"header_2.h" instead of #include"include/header_2.h"
        |-header_3.h  **If you dislike this behaviour, you can just modify main's Cmake file.
    |-main
        |-CMakeLists.txt **This is the main Cmake file, modify it when needed.
        |-data_fly_main.c **Where main lives.

    The code uses several libraries in the public domain. And the license is set accordingly.

    The features that should be supported by the data logger:
    -Reading data from CAN bus. (TWAI in Espressif documentation)
    -Writing data to sd-card. (With FAT32 File system)
    -Compressing data files. (TODO)
    -Uploading data files to a database. (TODO)
    -Heartbeat signals monitoring. (using MQTT protocol)
    -And some others.

    The Code is written in C (and partially C++). With ESP-idf as a main fraimwork. 
    The target chip is ESP32, even though it won't be hard to migrate into ESP32-S series 
    (Beware of ESP32-C series though, they usually come with one core. Some modifications are needed in task assigments).

    The initial plan is to use Core_1 for data reading and handling file system. While Core_0 will be responsible for 
    communication (HTTP, MQTT, and even compression). This way a lot of multithreading problems would be avoided.

    FreeRTOS Queues are used as a main data structure for communication between tasks. 

*/

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#include "sd_card.h"
#include "trigger_button.h"
#include "file_handle.h"
#include "can_node.h"
#include "can_node_mcp2515.h"


static const char *TAG = "DATA_FLY_MAIN_C";



void app_main(void)
{
    esp_err_t ret;

    // SD-card and SPI Initialisation.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = mountConfig();
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");
    sdmmc_host_t host = initializeSpi(&ret);

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);


    // CAN Driver Initialisation.
    // Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        printf("Driver installed\n");
    }
    else
    {
        printf("Failed to install driver\n");
        return;
    }

    // Start TWAI driver
    if (twai_start() == ESP_OK)
    {
        printf("Driver started\n");
    }
    else
    {
        printf("Failed to start driver\n");
        return;
    }    

// Initiate trigger related stuff.
    initBuzzerAndButton();
    createInterruptQueues();
    xTaskCreatePinnedToCore(triggerActive, "active trigger task", 2048, NULL, 0, NULL, 0);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(INPUT_PIN, gpio_interrupt_handler, (void *)INPUT_PIN);
   

    
    createFileErrQueue();
    createFileDataQueues();
    createFileNameQueue();

    createDirectory("Log_Fs");

    xTaskCreatePinnedToCore(&blinkFileErrorLED, "Blinking error led", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(&writeDataToFile, "Writing data to file", 8192, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(&SendCANData, "Send CAN data to file", 2048, NULL, 8, NULL, 1); 

    xTaskCreatePinnedToCore(&writeDataToErrorFiles, "Write CAN data to Error files", 8192, NULL, 0, NULL, 0);

    xTaskCreatePinnedToCore(&sendCanDataMCP2515, "Send MCP CAN data to be written", 2048, NULL, 0, NULL, 0); /// !!!!!!!ALWAYS KEEP THE PRIORITY OF THIS TASK AS 0!!!!!!!!!! ///
    xTaskCreatePinnedToCore(&writeDataToFileMCP, "Write MCP data to file", 8192, NULL, 10, NULL, 0);
    
    // Don't use any file operation after this point. 
    // All done, unmount partition and disable SPI peripheral

    // vTaskDelay(10000);
    // esp_vfs_fat_sdcard_unmount(mount_point, card);
    // ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    // spi_bus_free(host.slot);
    ESP_LOGI(TAG, "Exit app_main(void)");
}



 // const char *file_foo = MOUNT_POINT"/foo.txt";

    // // Check if destination file exists before renaming
    // struct stat st;
    // if (stat(file_foo, &st) == 0) {
    //     // Delete it if it exists
    //     unlink(file_foo);
    // }

    // Rename original file
    // ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    // if (rename(file_hello, file_foo) != 0) {
    //     ESP_LOGE(TAG, "Rename failed");
    //     return;
    // }

    // Open renamed file for reading
    // ESP_LOGI(TAG, "Reading file %s", file_foo);
    // f = fopen(file_foo, "r");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for reading");
    //     return;
    // }


    // Read a line from file
    // char line[64];
    // fgets(line, sizeof(line), f);
    // fclose(f);

    // Strip newline
    // char *pos = strchr(line, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);