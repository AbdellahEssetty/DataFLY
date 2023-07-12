#pragma once
#include "mcp2515.h"

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

void readCanDataMCP2515(void* params)
{
    struct can_frame can_message;
	vTaskDelay(2000 / portTICK_PERIOD_MS);
    CAN_Init();
	vTaskDelay(2000 / portTICK_PERIOD_MS);
    while(true)
    {
		// if(MCP2515_checkReceive())
		// {
            ERROR_t err_msg = MCP2515_readMessageAfterStatCheck(&can_message);
			if (err_msg == ERROR_OK)
			{
				printf("messages received\n");
				printf("CAN id: %ld === %03lX ---- CAN dlc: %u\nData: ", can_message.can_id, can_message.can_id, can_message.can_dlc);
				for (size_t i = 0; i < can_message.can_dlc; i++)
					printf("%02X ", can_message.data[i]);
				printf("\n");
			}
		     else if(err_msg == ERROR_NOMSG){
			ESP_LOGE("MAIN", "No messages received");
			}
            else {
                ESP_LOGW("MAIN", "Error code: %d", err_msg);
            }
		vTaskDelay(0);
    }
}

void sendCanDataMCP2515(void* params)
{
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    struct can_frame can_message;
    CAN_Init();
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    while(true)
    {
        ERROR_t err_msg = MCP2515_readMessageAfterStatCheck(&can_message);
        if (err_msg == ERROR_OK)
        {
            // printf("a");
            xQueueSend(file_data_queue_mcp2515, (void *) &can_message, portMAX_DELAY);
            taskYIELD();
        }
        else if (err_msg == ERROR_NOMSG) {
            // ESP_LOGW("CAN_NODE_MCP", "No messages received");
            taskYIELD();
        }
        else {
            ESP_LOGE("CAN_NODE_MCP", "Error code: %d", err_msg);
            break;
        }
		// taskYIELD();
    }
    vTaskDelete(NULL);
}