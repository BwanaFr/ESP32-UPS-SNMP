#include <OLED.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <logo.hpp>

//TODO: Use a global status object
#include <ETH.h>
#include <Temperature.hpp>
#include <UPSHIDDevice.hpp>
#include <DevicePins.hpp>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32



LogoAnimation::LogoAnimation() : step_(0)
{

}


bool LogoAnimation::animate(Adafruit_SSD1306& display)
{
    display.drawBitmap(0, 0, logo, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    switch (step_)
    {
    case 0:
        display.fillRect(40, 0, SCREEN_WIDTH-40, SCREEN_HEIGHT, SSD1306_BLACK);
        ++step_;
        break;
    case 1:
        display.fillRect(59, 0, SCREEN_WIDTH-59, SCREEN_HEIGHT, SSD1306_BLACK);
        ++step_;
        break;
    case 2:
        display.fillRect(69, 0, SCREEN_WIDTH-69, SCREEN_HEIGHT, SSD1306_BLACK);
        ++step_;
        break;
    case 3:
        display.fillRect(90, 0, SCREEN_WIDTH-90, SCREEN_HEIGHT, SSD1306_BLACK);
        ++step_;
        break;
    case 4:
        step_ = 0;
        break;
    default:
        step_ = 0;
        break;
    }
    display.display();
    if(step_ > 0){
        delay(200);
    }
    return step_ != 0;
}

Display::Display(uint8_t sclPin, uint8_t sdaPin) : 
    display_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1)
{
    Wire.setPins(sdaPin, sclPin);
}

void Display::begin()
{   
    xTaskCreate(
        oledTask,
        "oledTask",
        4096,
        (void*)this,
        2, NULL
    );
}

void Display::oledTask(void* param)
{
    Display* display = static_cast<Display*>(param);
    Wire.setTimeOut((uint16_t)2000);
    Wire.begin();
    if(!display->display_.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display->display_.clearDisplay();

    //Show animation
    LogoAnimation animation;
    while(animation.animate(display->display_)){};
    delay(3000);
    display->display_.setTextSize(1);      // Normal 1:1 pixel scale
    display->display_.setTextColor(SSD1306_WHITE); // Draw white text
    display->display_.cp437(true);         // Use full 256 char 'Code Page 437' font

    uint8_t pageNumber = 0;
    uint32_t nextDelay = 3000;
    for(;;){
        //Show IP and temperature
        display->display_.clearDisplay();
        switch(pageNumber){
            case 0:
                {
                    display->display_.setCursor(0, 0);
                    display->display_.printf("IP: %s", ETH.localIP().toString().c_str());
                    display->display_.setCursor(0, 12);
                    double temperature = tempProbe.getTemperature();
                    if(temperature != DEVICE_DISCONNECTED_C){
                        display->display_.printf("Temperature: %.1f C", temperature);
                    }else{
                        display->display_.printf("Temperature: -- C");
                    }
                    display->display_.setCursor(0, 24);
                    bool upsConnected = upsDevice.isConnected();
                    display->display_.printf("UPS: %s", upsConnected ? "CONNECTED" : "DISCONNECTED");
                    if(upsConnected){
                        ++pageNumber;
                        nextDelay = 3000;
                    }else{
                        nextDelay = 100;
                    }
                }
                break;
            case 1:
                {
                    display->display_.setCursor(0, 0);
                    if(upsDevice.getACPresent().isUsed()){
                        display->display_.printf("AC : %s", upsDevice.getACPresent().getValue() ? "PRESENT" : "NOT PRESENT");
                    }
                    display->display_.setCursor(0, 12);
                    if(upsDevice.getBatteryPresent().isUsed()){
                        display->display_.printf("Battery : %s", upsDevice.getBatteryPresent().getValue() ? "PRESENT" : "MISSING");
                    }
                    display->display_.setCursor(0, 24);
                    if(upsDevice.getRemainingCapacity().isUsed()){
                        display->display_.printf("Capacity : %d %%", (int32_t)upsDevice.getRemainingCapacity().getValue());
                    }
                    nextDelay = upsDevice.isConnected() ? 3000 : 0;
                    pageNumber = 0;
                }
                break;
           default:
                pageNumber = 0;
                nextDelay = 0;
                break;
        }
        if(nextDelay > 0){
            display->display_.display();
            unsigned long elapsed = 0;
            bool lastButtton = digitalRead(USER_BUTTON_PIN);
            while(elapsed < nextDelay){
                delay(20);
                elapsed += 20;
                //Failling edge detection
                bool btnState = digitalRead(USER_BUTTON_PIN);
                if(lastButtton && !btnState){
                    //Display next page
                    break;
                }
                lastButtton = btnState;
            }
        }
    }

    vTaskSuspend(NULL);
}

Display display(SCL_OLED_PIN, SDA_OLED_PIN);