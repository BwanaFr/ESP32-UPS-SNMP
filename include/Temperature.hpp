#ifndef _TEMPERATURE_HPP__
#define _TEMPERATURE_HPP__

#include <Arduino.h>
#include <FreeRTOS.h>
#include "driver/temperature_sensor.h"
#ifndef NO_TEMP_PROBE
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

class TemperatureProbe{
public:
    TemperatureProbe(uint8_t pin);
    virtual ~TemperatureProbe() = default;
    void begin();
#ifndef NO_TEMP_PROBE
    double getTemperatureProbe();
#endif
    float getInternalTemperature();
private:
#ifndef NO_TEMP_PROBE
    OneWire wire_;
    DallasTemperature dallas_;
#endif
    double temperature_;
    float internalTemperature_;
    SemaphoreHandle_t mutexData_;
    temperature_sensor_handle_t tempHandle;
    uint32_t failureCount_;
    void readTemperature();
    static void temperatureTask(void* param);
};

extern TemperatureProbe tempProbe;

#endif