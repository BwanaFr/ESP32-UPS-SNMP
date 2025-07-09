#ifndef _OLED_HPP__
#define _OLED_HPP__

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class LogoAnimation
{
public:
    LogoAnimation();
    virtual ~LogoAnimation() = default;
    bool animate(Adafruit_SSD1306& display);
private:
    uint16_t step_;
};

class Display
{
public:
    Display(uint8_t sclPin, uint8_t sdaPin);
    virtual ~Display() = default;
    void begin();
    static void oledTask(void* param);
private:
    Adafruit_SSD1306 display_;

};

extern Display display;

#endif