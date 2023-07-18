#pragma once
#include "mcp2515.h"
#include "can_encoder_decoder.h"
#include "cJSON.h"

#define HSPI_MISO 27
#define HSPI_MOSI 13
#define HSPI_CLK 14
#define HSPI_CS 15


bool HSPI_Init(void)
{
    ESP_LOGI("SPI_Init", "Hello from SPI_Init!");

    esp_err_t ret;

    // Configuration for the SPI bus
    spi_bus_config_t bus_cfg = {
        .miso_io_num = HSPI_MISO,
        .mosi_io_num = HSPI_MOSI,
        .sclk_io_num = HSPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0 // no limit
    };

    // Define MCP2515 SPI device configuration
    spi_device_interface_config_t dev_cfg = {
        .mode = 0, // (0,0)
        .clock_speed_hz = 10000000, // 10mhz
        .spics_io_num = HSPI_CS,
        .queue_size = 128
    };

    spi_device_handle_t spi_handle;
    // Initialize SPI bus
    ret = spi_bus_initialize(VSPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE("SPI_Init", "Failed to initialize SPI bus. Error: %d", ret);
        return false;
    }

    // Add MCP2515 SPI device to the bus
    ret = spi_bus_add_device(VSPI_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE("SPI_Init", "Failed to add SPI device. Error: %d", ret);
        return false;
    }

    MCP2515_Object->spi = spi_handle;

    return true;
}


void CAN_Init(void)
{
	MCP2515_init();
	HSPI_Init();
	MCP2515_reset();
	MCP2515_setBitrate(CAN_500KBPS, MCP_8MHZ);
	// MCP2515_setNormalMode();
    MCP2515_setListenOnlyMode();
	// xTaskCreatePinnedToCore(CAN_Module_RX_Task_Polling, "CAN_Module_RX_Task_Polling", 16384, NULL, 20, NULL, 1);
}

void sendCanDataMCP2515(void* params)
{
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    struct can_frame can_message;
    int dummy_channel_1 = 0;
    int dummy_channel_2 = 0;
    bool flag_channel_1 = false;
    bool flag_channel_2 = false;
    cJSON* cluster = cJSON_CreateObject();
    CAN_Init();
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    while(true)
    {
        ERROR_t err_msg = MCP2515_readMessageAfterStatCheck(&can_message);
        if (err_msg == ERROR_OK)
        {
            xQueueSend(file_data_queue_mcp2515, (void *) &can_message, portMAX_DELAY);
            if (can_message.can_id == 0x500 && ++dummy_channel_1 >= 500 && !flag_channel_1)
            {
                // ESP_LOGW("-", "-----------------------------------------------------");
                // ESP_LOGI("CAN_NODE_MCP", "Displaying Cluster");
                cJSON_AddNumberToObject(cluster, "Battery State Bars", decode(can_message.data, 7, 4, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Vehicle Speed", decode(can_message.data, 11, 7, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Odometer", decode(can_message.data, 25, 24, false, false, 0.1, 0));
                cJSON_AddNumberToObject(cluster, "State of Charge", decode(can_message.data, 18, 7, false, false, 0.5, 0));
                cJSON_AddNumberToObject(cluster, "Remaining Charge Time", decode(can_message.data, 49, 8, false, false, 2, 0));

                // ESP_LOGI("CAN_NODE_MCP", "Ready LCD %f", decode(can_message.data, 0, 1, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Autonomy Icon LCD %f", decode(can_message.data, 1, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Charge Icon LCD %f", decode(can_message.data, 3, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Drive Mode Selector LCD %f", decode(can_message.data, 5, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Batterry State bars %f", decode(can_message.data, 7, 4, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Vehicle Speed %f ", decode(can_message.data, 11, 7, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Battery State of Charge %f", decode(can_message.data, 18, 7, false, false, 0.5, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Odemeter %f", decode(can_message.data, 25, 24, false, false, 0.1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Remaining Charge Time %f", decode(can_message.data, 49, 8, false, false, 2, 0));
                // ESP_LOGW("-", "-----------------------------------------------------");
                dummy_channel_1 = 0;
                flag_channel_1 = true;
            }
            if (can_message.can_id == 0x510 && ++dummy_channel_2 >= 50 && !flag_channel_2)
            {
                // ESP_LOGW("CAN_NODE_MCP", "*****************");
                cJSON_AddNumberToObject(cluster, "Brake System Problem", decode(can_message.data, 6, 2, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Stop", decode(can_message.data, 10, 2, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Battery Temperature", decode(can_message.data, 14, 2, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Turtle Mode", decode(can_message.data, 18, 2, false, false, 1, 0));
                cJSON_AddNumberToObject(cluster, "Remaining Autonomy", decode(can_message.data, 34, 8, false, false, 1, 0));

                // ESP_LOGI("CAN_NODE_MCP", "Right Indicator %f", decode(can_message.data, 0, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Left Indicator %f", decode(can_message.data, 2, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Brake System Problem %f", decode(can_message.data, 6, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Stop %f", decode(can_message.data, 10, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Battery Temperature %f", decode(can_message.data, 14, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Turtle Mode %f", decode(can_message.data, 18, 2, false, false, 1, 0));
                // ESP_LOGI("CAN_NODE_MCP", "Remaining Autonomy %f", decode(can_message.data, 34, 8, false, false, 1, 0));
                // ESP_LOGW("CAN_NODE_MCP", "**************");
                dummy_channel_2 = 0;
                flag_channel_2 = true;
            }
            if (flag_channel_1 && flag_channel_2)
            {
                ESP_LOGI("CAN_NODE_MCP", "%s", cJSON_Print(cluster));
                cJSON_Delete(cluster);
                cluster = cJSON_CreateObject();
                flag_channel_1 = false;
                flag_channel_2 = false;
            }
        }
        else if (err_msg == ERROR_NOMSG) {
            // ESP_LOGW("CAN_NODE_MCP", "No messages received");
            taskYIELD();
        }
        else {
            ESP_LOGE("CAN_NODE_MCP", "Error code: %d", err_msg);
            // break;
        }
    }
    vTaskDelete(NULL);
}