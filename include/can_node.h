// 

void SendCANData(void *param)
{
    while(true)
    {
        //Wait for message to be received
        twai_message_t message;
        if (twai_receive(&message, pdMS_TO_TICKS(1000mn0)) == ESP_OK) {
            // ESP_LOGI("CAN_NODE_H", "Message received\n");
            xQueueSend(data_queue, (void *) &message, portMAX_DELAY);
        } else {
            ESP_LOGE("CAN_NODE_H", "Failed to receive message\n");
            break;
        }
        //Process received message
        // if (message.extd) {
        //     ESP_LOGI("CAN_NODE_H", "Message is in Extended Format\n");
        // } else {
        //     ESP_LOGI("CAN_NODE_H", "Message is in Standard Format\n");
        // }
        // ESP_LOGI("CAN_NODE_H", "ID is %ld\n", message.identifier);
        // if (!(message.rtr)) {
        //     for (int i = 0; i < message.data_length_code; i++) {
        //         printf("Data byte %d = %d\n", i, message.data[i]);
        //     }
        // }
        
        vTaskDelay(0);
    }
    vTaskDelete(NULL);
}