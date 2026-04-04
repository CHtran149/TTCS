#ifndef BLYNK_TASK_H
#define BLYNK_TASK_H

// Task function for sending data to Blynk
void TaskBlynk(void *pvParameters);

// Blynk virtual pin handler for energy reset
void setupBlynkHandlers();

#endif // BLYNK_TASK_H
