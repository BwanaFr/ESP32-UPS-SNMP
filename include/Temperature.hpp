#ifndef _TEMPERATURE_HPP__
#define _TEMPERATURE_HPP__

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FreeRTOS.h>

class TemperatureProbe{
public:
    TemperatureProbe(uint8_t pin);
    virtual ~TemperatureProbe() = default;
    void begin();
    double getTemperature();
    void loop();

private:
    OneWire wire_;
    DallasTemperature dallas_;
    double temperature_;
    SemaphoreHandle_t mutexData_;
    unsigned long lastRead_;
};

extern TemperatureProbe tempProbe;

#endif