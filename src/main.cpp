#include <Arduino.h>

#include <ETH.h>
#include <SPI.h>
#include <Webserver.hpp>
#include <WiFi.h>

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

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "UPSHIDDevice.hpp"
UPSHIDDevice upsDevice;


void WiFiEvent(arduino_event_id_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        //set eth hostname here
        ETH.setHostname("mdonze-esp32");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        webServer.setCredentials("TOTO", "LALA");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex()) {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");

        // After connecting, start the echo service
        Serial.println("Start web server");
        webServer.start();
        // start_webserver();
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected,Stop web server");
        webServer.start();
        // stop_webserver();
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        break;
    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);

    WiFi.onEvent(WiFiEvent);
    if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                   SPI3_HOST,
                   ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
        Serial.println("ETH start Failed!");
    }

#ifdef RGB_LED_PIN
    userLed.begin();
#endif
    hidBridge.onConfigDescriptorReceived = config_desc_cb;
    hidBridge.onDeviceInfoReceived = device_info_cb;
    hidBridge.onHidReportDescriptorReceived = hid_report_descriptor_cb;
    hidBridge.onReportReceived = hid_report_cb;
    hidBridge.onDeviceRemoved = device_removed_cb;
    hidBridge.begin();
    webServer.setup();    
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
    delay(10);

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