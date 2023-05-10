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

#include "freertos/queue.h"

static const uint8_t led_file = 33;
static esp_err_t err_file; 
static char count_file = '0';

// const char* TAG_H = "FILE_HANDLDE_H";

static QueueHandle_t err_queue = NULL;
static QueueHandle_t data_queue = NULL;


// Regular function: creating the queue for handling errors.

void createErrQueue()
{
    err_queue = xQueueCreate(10, sizeof(esp_err_t));
    if (!err_queue)
    {
        err_file = ESP_FAIL;
        if (xQueueSend(err_queue, (void *) &err_file, 10) != pdPASS)
        {
            gpio_set_level(led_file, 1);
        }
        ESP_LOGE(TAG_H, "Error Creating ERROR Queue");
    } else {
        ESP_LOGI(TAG_H, "Error queue created succesfully");
    }
}

// Regular function: creating the queue for passing data to files.
// This function is to be modified later, for causes of globality, the data is likely to be coming
// other source files, which makes passing data between them a little bit tricky.

void createDataQueue()
{
    // data_queue = xQueueCreate(64, sizeof(char));
    data_queue = xQueueCreate(10, sizeof(int));
    if (!data_queue)
    {
        err_file = ESP_FAIL;
        if (xQueueSend(err_queue, (void *) &err_file, 10) != pdPASS)
        {
            gpio_set_level(led_file, 1);
        }
        ESP_LOGE(TAG_H, "Error Creating DATA Queue");
    } else {
        ESP_LOGI(TAG_H, "Data Queue created succesfully");
    }
}


// Task: blinking LED whenever receiving an error type from the queue. 
// Other tasks (inside file handling) should always send errors to the queue, so all errors related 
// to the file system would show the same responce (for the user it is a blinking LED, the colour comes later).
// The LED shoud blink 15 times and then stops, I don't know wether this is a good approach.
// One possibility is to pass the number of blinks instead of the error, but it has some limitations (the error at this phrase cannot
// be terminated anyway, so restarting and modifying is necessarry. Moreover, one problem is when multiple task throw errors at the 
// same time).

void blinkErrorLED(void* pvParameter)
{
    // gpio_pad_select_gpio(led_file);
    gpio_set_direction(led_file, GPIO_MODE_OUTPUT);

    while (1)
    {
        if (xQueueReceive(err_queue, &err_file, portMAX_DELAY) == pdPASS)
        {
            ESP_LOGE(TAG_H, "An error has occured");
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
    ESP_LOGI(TAG_H, "Creating directory");
    if (err_file == ESP_OK)
    {
        ESP_LOGI(TAG_H, "Directory created");
    } else if (err_file == FR_EXIST)
    {
        ESP_LOGW(TAG_H, "Directory exists");
    } else
    {
        ESP_LOGE(TAG_H, "Error Creating Directory:");
        xQueueSend(err_queue, (void*) &err_file, portMAX_DELAY);
    }
}


// Regular function: Returning the name of the file to be ceated. Yeaaaaah another 10 line function to create the obvious, long live C. 
// This function is to be modified later, as the name of the file would strongly depend on the date of creation.
// For now it just appends an increasing number to the file. 
const char* getFileName()
{
    char* file_name = malloc(64);
    const char* constant_name = MOUNT_POINT"/LOG_FS/log_";
    strcpy(file_name, constant_name);
    file_name[strlen(constant_name)] = count_file++;
    file_name[strlen(constant_name) + 1] = '.';
    file_name[strlen(constant_name) + 2] = 'd';
    file_name[strlen(constant_name) + 3] = 'b';
    file_name[strlen(constant_name) + 4] = 'f';
    file_name[strlen(constant_name) + 5] = '\0';
    return file_name;
}


// Task: Create a file in sd-card, and write buffers that comes into the queue.
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

void writeFile(void* pvParameter)
{
    // vTaskDelay(100);
    const char *file_hello = getFileName();
    FILE *f = fopen(file_hello, "a");
    if (!f)
    {
        ESP_LOGE(TAG_H, "Failed to open file %s for writing", file_hello);
    } else {
        ESP_LOGI(TAG_H, "File created succesfully");
    }
    while (1)
    {
        int c;
        if (xQueueReceive(data_queue, &c, 10) != pdPASS)
        {
            xQueueSend(err_queue, (void*) &err_file, portMAX_DELAY);
            ESP_LOGI(TAG_H, "Error receiving data from the queue");
            break;
        } else {
            fprintf(f, "%d\n", c);
            ESP_LOGI(TAG_H, "Receiving %d", c);
            // f.close();
            // break
        }
        if(c == 1000)
        {
            fclose(f);
            break;
        }
        vTaskDelay(0);
        
    }
    vTaskDelete(NULL);
}

void sendDataToWrite(void* pvParameter)
{
    int count = 0;
    while (1)
    {
        // const char* text = "Strangers passing in the street";
        ESP_LOGI(TAG_H, "Sending %d", count);
        xQueueSend(data_queue, (void *) &count, portMAX_DELAY);
        // for (size_t i = 0; i < 5; i++)
        // {
        //     // xQueueSend(data_queue, (void *) &i, 10);
        //     gpio_set_level(led_file, 1);
        //     vTaskDelay(10);
        //     gpio_set_level(led_file, 0);
        //     vTaskDelay(10);
        // }
        count++;
        // taskYIELD();
        vTaskDelay(1);
    }  
    vTaskDelete(NULL);
}
