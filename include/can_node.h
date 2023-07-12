// 
#pragma once

void SendCANData(void *param)
{
    while(true)
    {
        //Wait for message to be received
        twai_message_t message;
        if (twai_receive(&message, pdMS_TO_TICKS(10000)) == ESP_OK) {
            // ESP_LOGI("CAN_NODE_H", "Message received\n");
            xQueueSend(file_data_queue, (void *) &message, portMAX_DELAY);
        } else {
            ESP_LOGE("CAN_NODE_H", "Failed to receive message\n");
            break;
        }
        vTaskDelay(0);
    }
    vTaskDelete(NULL);
}