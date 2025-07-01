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

#define DAEMON_TASK_LOOP_DELAY  3 // ticks
#define CLASS_TASK_LOOP_DELAY   3 // ticks
#define DAEMON_TASK_COREID      0
#define CLASS_TASK_COREID       0
#include "usb_host_hid_bridge.h"

void config_desc_cb(const usb_config_desc_t *config_desc);
void device_info_cb(usb_device_info_t *dev_info);
void hid_report_descriptor_cb(usb_transfer_t *transfer);
void hid_report_cb(usb_transfer_t *transfer);
void device_removed_cb();

UsbHostHidBridge hidBridge;
int32_t usb_input_ch[] = { 0,0,0,0, 0,0,0,0 };


UPSHIDDevice upsDevice;
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
            config.getDeviceName(deviceName);
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
    config.begin();
    //Loads configuration from flash (Default used if flash empty)
    config.load();

    //Starts Ethernet
    WiFi.onEvent(WiFiEvent);
    if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                   SPI3_HOST,
                   ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
        ESP_LOGE(TAG, "ETH start Failed!");
        //TODO: Flash LED in RED
    }
    IPAddress ip, subnet, gateway;
    config.getIPAddress(ip, subnet, gateway);
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
    //TODO: Move this to the UPSHIDDevice object
    hidBridge.onConfigDescriptorReceived = config_desc_cb;
    hidBridge.onDeviceInfoReceived = device_info_cb;
    hidBridge.onHidReportDescriptorReceived = hid_report_descriptor_cb;
    hidBridge.onReportReceived = hid_report_cb;
    hidBridge.onDeviceRemoved = device_removed_cb;
    hidBridge.begin();

    //Setup the web server
    webServer.setup();

    //Register a listener to know configuration changes
    config.registerListener(configChanged);
}

void loop()
{
    unsigned long now = millis();
#ifdef RGB_LED_PIN
    static unsigned long lastRGB = 0;
    static float hue = 0.0f;
    userLed.loop();
    if((now - lastRGB) >= 20){
        userLed.customHSV(hue, 1.0, 0.2);
        hue += 10;
        if(hue > 360){
          hue = 0;
        }
        lastRGB = now;
    }
#endif
    snmpAgent.loop();
    config.loop();
}

void config_desc_cb(const usb_config_desc_t *config_desc) {
    usb_print_config_descriptor(config_desc, NULL);
}

void device_info_cb(usb_device_info_t *dev_info) {
    if (dev_info->str_desc_manufacturer) usb_print_string_descriptor(dev_info->str_desc_manufacturer);
    if (dev_info->str_desc_product)      usb_print_string_descriptor(dev_info->str_desc_product);
    if (dev_info->str_desc_serial_num)   usb_print_string_descriptor(dev_info->str_desc_serial_num);
}

void hid_report_descriptor_cb(usb_transfer_t *transfer) {
    //>>>>> for HID Report Descriptor
    // Explanation: https://electronics.stackexchange.com/questions/68141/
    // USB Descriptor and Request Parser: https://eleccelerator.com/usbdescreqparser/#
    //<<<<<
    // Serial.printf("\nstatus %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);
    // for(int i=0; i < transfer->actual_num_bytes; i++) {
    //     if (i == USB_SETUP_PACKET_SIZE) {
    //         Serial.printf("\n\n>>> Goto https://eleccelerator.com/usbdescreqparser/ \n");
    //         Serial.printf(">>> Copy & paste below HEX and parser as... USB HID Report Descriptor\n\n");
    //     }
    //     Serial.printf("%02X ", transfer->data_buffer[i]);
    // }
    // Serial.printf("\n\n");
    // Serial.printf("HID Report Descriptor\n");
    uint8_t *const data = (uint8_t *const)(transfer->data_buffer + USB_SETUP_PACKET_SIZE);
    size_t len = transfer->actual_num_bytes - USB_SETUP_PACKET_SIZE;

    upsDevice.buildFromHIDReport(data, len);
    // Serial.printf("> size: %ld bytes\n", len);
}

void hid_report_cb(usb_transfer_t *transfer) {
    //
    // check HID Report Descriptor for usage
    //
    uint8_t *data = (uint8_t *)(transfer->data_buffer);

    // Serial.println("hid_report_cb");
    // for (int i=0; i<transfer->actual_num_bytes && i<11; i++) {
    //     // Serial.printf("%d ", data[i]);
    //     Serial.printf("%02X ", data[i]);
    //     // for (int b=0; b<8; b++) Serial.printf("%d", (data[i] & (1 << b)) >> b );
    //     // Serial.printf(" ");
    // }
    // Serial.printf("\n\t");
    upsDevice.hidReportData(data, transfer->actual_num_bytes);
    // usb_input_ch[0] = data[0];
    // usb_input_ch[1] = data[1];
    // usb_input_ch[2] = data[2];
    // usb_input_ch[3] = data[3];
    // for (int i=0; i<4; i++) Serial.printf("%d ", usb_input_ch[i]);
    // Serial.printf("\n");

    // usb_input_ch[0] = get_offset_bits(data, 8,  8);
    // usb_input_ch[1] = get_offset_bits(data, 16, 8);
    // usb_input_ch[2] = get_offset_bits(data, 24, 8);
    // usb_input_ch[3] = get_offset_bits(data, 32, 8);
    // for (int i=0; i<4; i++) Serial.printf("%d ", usb_input_ch[i]);
    // Serial.printf("\n");
}

void device_removed_cb() {
    upsDevice.deviceRemoved();
}