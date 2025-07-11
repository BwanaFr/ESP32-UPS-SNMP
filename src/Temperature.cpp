#include <Temperature.hpp>

#include "esp_log.h"
#include <FreeRTOS.h>

static const char *TAG = "TempProbe";

#ifndef TEMP_PROBE_PIN
#define TEMP_PROBE_PIN 15
#endif

#ifndef TEMP_READ_PERIOD
#define TEMP_READ_PERIOD 500
#endif

TemperatureProbe::TemperatureProbe(uint8_t pin) : 
    wire_(pin), dallas_(&wire_), temperature_(0.0)
{
    mutexData_ = xSemaphoreCreateMutex();
    if(mutexData_ == NULL){
        ESP_LOGE(TAG, "Unable to create data mutex");
    }
}

void TemperatureProbe::begin()
{
    DeviceAddress sensorDeviceAddress;
    dallas_.begin();
    dallas_.getAddress(sensorDeviceAddress, 0);
    dallas_.setResolution(sensorDeviceAddress, 10);
    xTaskCreate(temperatureTask, "tempTask", 2048, (void*)this,  0, NULL);
}

double TemperatureProbe::getTemperature()
{
    double ret = 0.0;
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ret = temperature_;
        xSemaphoreGive(mutexData_);
    }    
    return ret;
}

void TemperatureProbe::readTemperature()
{
    for(;;){
        dallas_.requestTemperatures(); 
        if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
        {
            temperature_ = dallas_.getTempCByIndex(0);
            xSemaphoreGive(mutexData_);
        }
        delay(500);
    }
}

void TemperatureProbe::temperatureTask(void* param)
{
    TemperatureProbe* app = static_cast<TemperatureProbe*>(param);
    app->readTemperature();
}

TemperatureProbe tempProbe(TEMP_PROBE_PIN);