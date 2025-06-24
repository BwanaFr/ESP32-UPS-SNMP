/*
**    This software license is not yet defined.
**
*/
#ifndef _USER_LED_HPP__
#define _USER_LED_HPP__
#ifdef RGB_LED_PIN
#include <Arduino.h>
#include <FastLED.h>

/**
 * Simple class for managing user LED
*/
class UserLed {
public:
    /**
     * Constructor
    */
    UserLed();

    ~UserLed() = default;

    void begin();
    void loop();
    void error();
    void warning();
    void clearError();
    void customColor(uint8_t r, uint8_t g, uint8_t b);
    void customHSV(float hue, float saturation, float value);
    static inline UserLed* getInstance(){  return instance_; }
private:
    enum class State {CLEAR, ERROR, WARNING, CUSTOM};
    State state_;
    struct NewState{
        State state;
        uint32_t payload;
    };
    CRGB led_;
    SemaphoreHandle_t semaphore_;   //Semaphore to share ressource
    QueueHandle_t newStateQueue_;
    static UserLed* instance_;

    void setNewState(State newState, u_int32_t payload = 0);
    State getState();
    static void HSVToRGB(float hue, float saturation, float value, uint8_t& red, uint8_t& green, uint8_t& blue);
};
#endif
#endif
