// May 3rd 2023, Written by Abdellah ESSETTY.
// The aim of this header file is to provide an interface for file handling using the FreeRTOS architecture. 
// This header file should be able to create, rename, replace, and delete files and directories using the FAT FS API.
// Official documentation for FATFS and its API http://elm-chan.org/fsw/ff/00index_e.html
// A user might just use the POSIX with the C library, or just use the FATFS API, they are almost identical.

// For error handling, a static global (to be used in the scope of this file only) error variable will be used. 
// At any point an error occur in this file (which means an error happened at the level of file handling) the user should 
// notified via an LED, and in serial terminal. 

// A queue is also used for passing data from a one place to another, the file writing would use the queue to get 
// data from the CAN bus, and then pass it to the file. Following this approach provide a thread safe, atomic 
// behaviour for writing. (Time constraints and rapidity to be mesured later).
#pragma once
#include "freertos/queue.h"
#include "esp_timer.h"

static const uint8_t led_file = 33;
static esp_err_t err_file; 
static int count_file = 3000;

// const char* "FILE_HANDLE_H" = "FILE_HANDLDE_H";

static QueueHandle_t file_err_queue = NULL;
static QueueHandle_t file_data_queue = NULL;


// Regular function: creating the queue for handling errors.

void createFileErrQueue()
{
    file_err_queue = xQueueCreate(10, sizeof(esp_err_t));
    if (!file_err_queue)
    {
        err_file = ESP_FAIL;
        gpio_set_level(led_file, 1);
        ESP_LOGE("FILE_HANDLE_H", "Error Creating ERROR Queue");
    } else {
        ESP_LOGI("FILE_HANDLE_H", "Error queue created succesfully");
    }
}

// Regular function: creating the queue for passing data to files.
// This function is to be modified later, for causes of globality, the data is likely to be coming
// other source files, which makes passing data between them a little bit tricky.

void createFileDataQueue()
{
    // file_data_queue = xQueueCreate(64, sizeof(char));
    file_data_queue = xQueueCreate(100, sizeof(twai_message_t));
    if (!file_data_queue)
    {
        err_file = ESP_FAIL;
        if (xQueueSend(file_err_queue, (void *) &err_file, 10) != pdPASS)
        {
            gpio_set_level(led_file, 1);
        }
        ESP_LOGE("FILE_HANDLE_H", "Error Creating DATA Queue");
    } else {
        ESP_LOGI("FILE_HANDLE_H", "Data Queue created succesfully");
    }
}


// Task: blinking LED whenever receiving an error type from the queue. 
// Other tasks (inside file handling) should always send errors to the queue, so all errors related 
// to the file system would show the same responce (for the user it is a blinking LED, the colour comes later).
// The LED shoud blink 15 times and then stops, I don't know wether this is a good approach.
// One possibility is to pass the number of blinks instead of the error, but it has some limitations (the error at this phrase cannot
// be terminated anyway, so restarting and modifying is necessarry. Moreover, one problem is when multiple task throw errors at the 
// same time).

void blinkFileErrorLED(void* pvParameter)
{
    // gpio_pad_select_gpio(led_file);
    gpio_set_direction(led_file, GPIO_MODE_OUTPUT);

    while (1)
    {
        if (xQueueReceive(file_err_queue, &err_file, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGE("FILE_HANDLE_H", "An error has occured");
            for (size_t i = 0; i < 15; i++)
            {
                gpio_set_level(led_file, 1);
                vTaskDelay(400 / portTICK_PERIOD_MS);
                gpio_set_level(led_file, 0);
                vTaskDelay(400 / portTICK_PERIOD_MS);
            }
        }
    }
}


// Regular function: Creating a directory with a given name.
// The function throws an error to the queue if it cannot create a directory.

void createDirectory(const char* file_name_)
{
    err_file = f_mkdir(file_name_);
    ESP_LOGI("FILE_HANDLE_H", "Creating directory");
    if (err_file == ESP_OK)
    {
        ESP_LOGI("FILE_HANDLE_H", "Directory created");
    } else if (err_file == FR_EXIST)
    {
        ESP_LOGW("FILE_HANDLE_H", "Directory exists");
    } else
    {
        ESP_LOGE("FILE_HANDLE_H", "Error Creating Directory:");
        xQueueSend(file_err_queue, (void*) &err_file, portMAX_DELAY);
    }
}


// Regular function: Returning the name of the file to be ceated. Yeaaaaah another 10 line function to create the obvious, long live C. 
// This function is to be modified later, as the name of the file would strongly depend on the date of creation.
// For now it just appends an increasing number to the file. 

const char* getFileName(bool is_data)
{
    char int_str[32];
        
    char* file_name = malloc(64);
    char* constant_name = is_data ? MOUNT_POINT"/LOG_FS/log_" : MOUNT_POINT"/ERR_FS/log_";
    strcpy(file_name, constant_name);

    sprintf(int_str, "%d", count_file++);
    strcat(file_name, int_str);
    strcat(file_name, ".asc");
    return file_name;
}

/// @brief send error messages to error data queues for a specific duration (2 minutes)
/// @param message TWAI (CAN) message to send
/// @param send_err_messages a bool variable showing whether error messages should be sent or not.
/// @param start_time starting time of sending error messages. 
void sendErrorMessagesDuration(twai_message_t* message, bool* send_err_messages, int64_t* start_time)
{
    // int64_t start_time = esp_timer_get_time();
    int64_t end_time = esp_timer_get_time();
    int64_t time_difference = end_time - *start_time;
    int64_t duration = 1 * 60 * 1e6;
    if(time_difference < duration)
        xQueueSend(trigger_err_data_queue, (void*) message, 0);
    else
        *send_err_messages = false;
}

/// @brief Task: Create a file in sd-card, and write buffers that comes into the queue.
// This task is to be modified (for compatibility reasons).
// Some few important details about writing data to files.
// File creation should be based on time (grab time from GPS module and name the file after it).
// A specefic folder should keep all the files.
// Data would be appended to the file based on reception of data from the queue (or some other data structure).
// The file hierarchy should look something like this:
// /root
//        /Folder: Log Files
//              /File: log_15_30_03_05_2023.dbf
//              /File: log_10_12_04_05_2023.dbf
// 
// The file should be closed:
//      -If some specific time has passed without receiving any data (shutdown, CAN bus sleep...).
//      -If an error has occured when receiving data.
//      -If the file has reached some size limit.
// Alternatively, a new file should be created:

void writeDataToFile(void* pvParameter)
{
    // vTaskDelay(100);
    const char* file_name = getFileName(true);
    int number_of_lines = 0;
    bool send_err_messages = false;
    FILE *f = fopen(file_name, "a");
    if (!f)
    {
        ESP_LOGE("FILE_HANDLE_H", "Failed to open file %s for writing", file_name);
    } else {
        ESP_LOGI("FILE_HANDLE_H", "File %s created succesfully", file_name);
    }
    while (true)
    {
        twai_message_t message;
        int listen_message;
        int64_t start_err_msg_time;
        if (xQueueReceive(file_data_queue, &message, 10000) != pdPASS)
        {
            xQueueSend(file_err_queue, (void*) &err_file, portMAX_DELAY);
            ESP_LOGE("FILE_HANDLE_H", "Error receiving data from the queue");
            break;
        } else {
            char data_or_request = message.rtr ? 'r' : 'd';
            fprintf(f, " %f 1        %03lX             Tx   %c %d", (double) esp_timer_get_time()*1e-6, 
            message.identifier, data_or_request, message.data_length_code);
            for (int i = 0; i < message.data_length_code; i++) 
                fprintf(f, " %02X", message.data[i]);
            fprintf(f, "\n");
            
            // --------------- Check for errors -----------------//
            if(xQueueReceive(trigger_listen_queue, (void*) &listen_message, 0))
            {
                send_err_messages = true;
                start_err_msg_time = esp_timer_get_time();
            }
            if(send_err_messages)
                sendErrorMessagesDuration(&message, &send_err_messages, &start_err_msg_time);
                
        }
        if(number_of_lines++ == 1000) //A condition to save the file. If not closed, all modification would not be saved.
        {
            fclose(f);
            ESP_LOGI("FILE_HANDLE_H", "file Closed");
            f = fopen(file_name, "a");
            number_of_lines = 0;
        }
        vTaskDelay(0);
    }
    fclose(f);
    vTaskDelete(NULL);
}

void writeDataToErrorFiles(void* pvParameter)
{
    int number_of_lines = 0;
    const char* file_name = getFileName(false);
    twai_message_t message;
    FILE *f = fopen(file_name, "w");
    while (true)
    {

        if(xQueueReceive(trigger_err_data_queue, (void*) &message, portMAX_DELAY) == pdPASS)
        {
            char data_or_request = message.rtr ? 'r' : 'd';
            fprintf(f, " %f 1        %03lX             Tx   %c %d", (double) esp_timer_get_time()*1e-6, 
            message.identifier, data_or_request, message.data_length_code);
            for (int i = 0; i < message.data_length_code; i++) 
                fprintf(f, " %02X", message.data[i]);
            fprintf(f, "\n");
            if(number_of_lines++ == 1000) //A condition to save the file. If not closed, all modification would not be saved.
            {
                fclose(f);
                ESP_LOGI("FILE_HANDLE_H", "Error file Closed");
                f = fopen(file_name, "a");
                number_of_lines = 0;
            }
            vTaskDelay(0);
        }
        // fclose(f);
    }
    fclose(f);
    vTaskDelete(NULL);
}

// void sendDataToWrite(void* pvParameter)
// {
//     int count = 0;
//     while (1)
//     {
//         // const char* text = "Strangers passing in the street";
//         ESP_LOGI("FILE_HANDLE_H", "Sending %d", count);
//         xQueueSend(file_data_queue, (void *) &count, portMAX_DELAY);
//         // for (size_t i = 0; i < 5; i++)
//         // {
//         //     // xQueueSend(file_data_queue, (void *) &i, 10);
//         //     gpio_set_level(led_file, 1);
//         //     vTaskDelay(10);
//         //     gpio_set_level(led_file, 0);
//         //     vTaskDelay(10);
//         // }
//         count++;
//         // taskYIELD();
//         vTaskDelay(1);
//     }  
//     vTaskDelete(NULL);
// }
