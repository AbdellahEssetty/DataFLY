// Use Case: 
// Button clicked -> ISR -> trigger_interrupt_queue -> TriggerActive(task) -> tonic sone
//                                                                        |-> trigger_listen_queue -> writeDataToFile(file_hanlde.h task) -> trigger_err_data_queue_spi
//                                                                                                                             -> trigger_err_data_queue -> writeDataToErrFile(task).

#pragma once

#include "mcp2515.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define INPUT_PIN 25
#define BUZZER_PIN 32
#define DEBOUNCE_DELAY_MS 500

// getting data from ISR and notice user.
static QueueHandle_t trigger_interrupt_queue = NULL;
// Notifying the writeDataToFile task.
static QueueHandle_t trigger_listen_queue = NULL;
// TWAI Controller Error data queue. 
static QueueHandle_t trigger_err_data_queue = NULL;
// MCP2515 Controller Error data queue.
static QueueHandle_t trigger_err_data_queue_mcp2515 = NULL;
// TODO: implement a second error data queue for CAN data comming from the MCP2515 Controller (using HSPI and a third party library).
static esp_err_t err_trigger;

void createInterruptQueues()
{
    trigger_interrupt_queue = xQueueCreate(1, sizeof(int));
    trigger_listen_queue = xQueueCreate(1, sizeof(int));
    trigger_err_data_queue = xQueueCreate(100, sizeof(twai_message_t));
    trigger_err_data_queue_mcp2515 = xQueueCreate(100, sizeof(struct can_frame));
    if (!(trigger_err_data_queue && trigger_interrupt_queue))
    {
        err_trigger = ESP_FAIL;
        gpio_set_level(BUZZER_PIN, 1);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        gpio_set_level(BUZZER_PIN, 0);
        ESP_LOGE("FILE_HANDLE_H", "Error Creating Interrupt Queues");
    } else {
        ESP_LOGI("FILE_HANDLE_H", "All Interrupt Queues have been created succesfully");
    }
}

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    xQueueSendFromISR(trigger_interrupt_queue, &pinNumber, NULL);
}

void triggerActive(void *params)
{
    int pinNumber, count = 0, trigger = 0;
    TickType_t lastClickTime = 0;
    while (true)
    {
        if (xQueueReceive(trigger_interrupt_queue, &pinNumber, portMAX_DELAY) == pdTRUE)
        {
            TickType_t currentTime = xTaskGetTickCount();
            if ((currentTime - lastClickTime) >= pdMS_TO_TICKS(DEBOUNCE_DELAY_MS))
            {
                lastClickTime = currentTime;
                printf("GPIO %d was pressed %d times.\n", pinNumber, count++);
                xQueueSend(trigger_listen_queue, (void*) &trigger, 10);
                gpio_set_level(BUZZER_PIN, 1);
                xQueueReceive(trigger_interrupt_queue, &pinNumber, 0); // A weird way to debounce.
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                xQueueReceive(trigger_interrupt_queue, &pinNumber, 0); // A weird way to debounce.
                gpio_set_level(BUZZER_PIN, 0);
            }
            vTaskDelay(0);
        }
        vTaskDelay(0);
    }
}

void initBuzzerAndButton()
{
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(INPUT_PIN);
    gpio_pullup_en(INPUT_PIN);
    gpio_set_intr_type(INPUT_PIN, GPIO_INTR_POSEDGE);
}

