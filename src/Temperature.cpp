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

#ifndef TEMP_FAILURE_MAX
#define TEMP_FAILURE_MAX 5
#endif

#ifndef NO_TEMP_PROBE
TemperatureProbe::TemperatureProbe(uint8_t pin) : 
    wire_(pin), dallas_(&wire_), temperature_(0.0), failureCount_(0)
#else
TemperatureProbe::TemperatureProbe(uint8_t pin) : 
    temperature_(0.0), failureCount_(0)
#endif
{
    temperature_sensor_config_t temp_sensor = {
            .range_min = -10,
            .range_max = 80,
        };
    temperature_sensor_install(&temp_sensor, &tempHandle);
    temperature_sensor_enable(tempHandle);

    mutexData_ = xSemaphoreCreateMutex();
    if(mutexData_ == NULL){
        ESP_LOGE(TAG, "Unable to create data mutex");
    }
}

void TemperatureProbe::begin()
{
#ifndef NO_TEMP_PROBE
    DeviceAddress sensorDeviceAddress;
    dallas_.begin();
    dallas_.getAddress(sensorDeviceAddress, 0);
    dallas_.setResolution(sensorDeviceAddress, 10);
#endif
    xTaskCreate(temperatureTask, "tempTask", 2048, (void*)this,  0, NULL);
}

#ifndef NO_TEMP_PROBE
double TemperatureProbe::getTemperatureProbe()
{
    double ret = 0.0;
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ret = temperature_;
        xSemaphoreGive(mutexData_);
    }    
    return ret;
}
#endif

float TemperatureProbe::getInternalTemperature()
{
    float ret = 0.0;
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ret = internalTemperature_;
        xSemaphoreGive(mutexData_);
    }    
    return ret;
}

void TemperatureProbe::readTemperature()
{
    for(;;){
#ifndef NO_TEMP_PROBE
        dallas_.requestTemperatures(); 
        float tempRead = dallas_.getTempCByIndex(0);
        if(tempRead == DEVICE_DISCONNECTED_C){
            if(failureCount_ < TEMP_FAILURE_MAX){
                ++failureCount_;
            }
        }else{
            failureCount_ = 0;
        }
#endif
        //Read internal temperature
        float intTemp = 0.0;
        temperature_sensor_get_celsius(tempHandle, &intTemp);
        if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
        {
#ifndef NO_TEMP_PROBE
            if(failureCount_ < TEMP_FAILURE_MAX){
                //Read ok
                temperature_ = tempRead;
            }else{
                temperature_ = DEVICE_DISCONNECTED_C;
            }
#endif
            internalTemperature_ = intTemp;
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
