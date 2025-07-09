#include <Arduino.h>

#include <ETH.h>
#include <SPI.h>
#include <Webserver.hpp>
#include <WiFi.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "UPSSNMP.hpp"
#include "UPSHIDDevice.hpp"
#include "Configuration.hpp"
#include "Temperature.hpp"
#include "OLED.hpp"

//Pins defs for W5500 chip
#define ETH_MOSI_PIN 11
#define ETH_MISO_PIN 12
#define ETH_SCLK_PIN 13
#define ETH_CS_PIN 14
#define ETH_INT_PIN 10
#define ETH_RST_PIN 9

#ifdef RGB_LED_PIN
#include <UserLed.hpp>
static UserLed userLed;
#endif

UPSSNMPAgent snmpAgent;


static const char* TAG = "Main";

/**
 * Configuration changed
 */
void configChanged(DeviceConfiguration::Parameter what)
{
    switch(what){
        case DeviceConfiguration::Parameter::DEVICE_NAME:
            std::string deviceName;
            Configuration.getDeviceName(deviceName);
            ETH.setHostname(deviceName.c_str());
            ESP_LOGI(TAG, "New device name! %s", deviceName.c_str());
    }
}

void WiFiEvent(arduino_event_id_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        ESP_LOGI(TAG, "ETH Started");
        //Sets hostname
        {
            std::string deviceName;
            Configuration.getDeviceName(deviceName);
            ETH.setHostname(deviceName.c_str());
        }
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        ESP_LOGI(TAG, "ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        ESP_LOGI(TAG, "ETH MAC: %s, IPv4: %s, Duplex: %s, %uMbps", ETH.macAddress().c_str(), ETH.localIP().toString().c_str(), 
                    ETH.fullDuplex() ? "FULL" : "HALF", ETH.linkSpeed());
        //Ethernet available starts network services
        webServer.start();
        snmpAgent.start();
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        ESP_LOGI(TAG, "ETH Disconnected, Stop services");
        webServer.stop();
        snmpAgent.stop();
        break;
    case ARDUINO_EVENT_ETH_STOP:
        ESP_LOGI(TAG, "ETH Stopped");
        break;
    default:
        break;
    }
}

void setup()
{
    //Starts serial
    Serial.begin(115200);
    
    //Initializes configuration
    Configuration.begin();
    //Loads configuration from flash (Default used if flash empty)
    Configuration.load();
    //Starts Dallas probe
    tempProbe.begin();

    //Starts Ethernet
    WiFi.onEvent(WiFiEvent);
    if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                   SPI3_HOST,
                   ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
        ESP_LOGE(TAG, "ETH start Failed!");
        //TODO: Flash LED in RED
    }
    IPAddress ip, subnet, gateway;
    Configuration.getIPAddress(ip, subnet, gateway);
    if(ip == INADDR_NONE){
        ESP_LOGI(TAG, "Using DHCP");
    }else{
        ESP_LOGI(TAG, "Setting IP to %s %s %s", ip.toString().c_str(), subnet.toString().c_str(), gateway.toString().c_str());
        ETH.config(ip, gateway, subnet);
    }

#ifdef RGB_LED_PIN
    //Starts the user led task
    userLed.begin();
#endif
    //Configure HID bridge
    upsDevice.begin();

    //Setup the web server
    webServer.setup();

    //Register a listener to know configuration changes
    Configuration.registerListener(configChanged);

    //Starts OLED display
    display.begin();
}

void loop()
{
    unsigned long now = millis();
#ifdef RGB_LED_PIN
    static unsigned long lastRGB = 0;
    static float hue = 0.0f;
    userLed.loop();
    if((now - lastRGB) >= 100){
        userLed.customHSV(hue, 1.0, 0.2);
        hue += 10;
        if(hue > 360){
          hue = 0;
        }
        lastRGB = now;
    }
#endif
    snmpAgent.loop();
    Configuration.loop();
    tempProbe.loop();
}
