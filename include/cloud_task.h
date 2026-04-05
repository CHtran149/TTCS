#ifndef CLOUD_TASK_H
#define CLOUD_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Struct to hold the values to send to cloud (date will be generated at send time
// if epoch==0)
typedef struct {
    uint32_t epoch; // seconds since Unix epoch (optional, 0 = use current time)
    float voltage;
    float current;
    float power;
    float energy;
} CloudData;

// Task entry
void TaskCloud(void *pvParameters);

// Queue handle (defined in main.cpp)
extern QueueHandle_t cloud_queue;

#endif // CLOUD_TASK_H
