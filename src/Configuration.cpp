#include <Configuration.hpp>
#include "esp_log.h"
#include <LittleFS.h>

#define DEFAULT_TEMPERATURE_ALARM 65.0
#define FLASH_SAVE_DELAY 1000
#define CFG_FILENAME "/config.json"

const char* CFGTAG = "Configuration";

DeviceConfiguration Configuration;

DeviceConfiguration::DeviceConfiguration() : 
        lastChange_(0), tempAlarm_(DEFAULT_TEMPERATURE_ALARM),
        ip_(INADDR_NONE), subnet_(INADDR_NONE), gateway_(INADDR_NONE)
{
    mutexData_ = xSemaphoreCreateMutex();
    if(mutexData_ == NULL){
        ESP_LOGE(CFGTAG, "Unable to create data mutex");
    }

    mutexListeners_ = xSemaphoreCreateMutex();
    if(mutexListeners_ == NULL){
        ESP_LOGE(CFGTAG, "Unable to create listeners mutex");
    }
}

void DeviceConfiguration::begin()
{
    LittleFS.begin(true);
}

void DeviceConfiguration::load()
{
    File configFile = LittleFS.open(CFG_FILENAME, "r");
    if(configFile){
        JsonDocument json;
        if(deserializeJson(json, configFile) == DeserializationError::Ok) {
            fromJSON(json);
            lastChange_ = 0;    //Don't write to flash
        }
        configFile.close();
    }
}

void DeviceConfiguration::fromJSON(const JsonDocument& doc)
{
    if(doc["DeviceName"]){
        setDeviceName(doc["DeviceName"]);
    }
    
    IPAddress ip;
    IPAddress subnet;
    IPAddress gateway;
    getIPAddress(ip, subnet, gateway);

    if(doc["IP"]){
        std::string ipStr = doc["IP"];
        if(ipStr != "DHCP"){
            ip.fromString(ipStr.c_str());
        }else{
            ip = INADDR_NONE;
        }
    }
    if(doc["Subnet"]){
        std::string ipStr = doc["Subnet"];
        if(ipStr != "DHCP"){
            subnet.fromString(ipStr.c_str());
        }else{
            subnet = INADDR_NONE;
        }
    }
    if(doc["Gateway"]){
        std::string ipStr = doc["Gateway"];
        if(ipStr != "DHCP"){
            gateway.fromString(ipStr.c_str());
        }else{
            gateway = INADDR_NONE;
        }
    }
    setIPAddress(ip, subnet, gateway);

    if(doc["TemperatureMax"]){
        setTemperatureAlarm(doc["TemperatureMax"]);
    }
}

void DeviceConfiguration::setDeviceName(const std::string& name)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        deviceName_ = name;
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::DEVICE_NAME);
    }
}

void DeviceConfiguration::getDeviceName(std::string& name)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        name = deviceName_;
        xSemaphoreGive(mutexData_);
    }
}

void DeviceConfiguration::setIPAddress(const IPAddress& ip, const IPAddress& subnet, const IPAddress& gateway)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ip_ = ip;
        subnet_ = subnet;
        gateway_ = gateway;
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::IP_CONFIGURATION);
    }
}

/**
 * Gets IP address of this device
 * @param ip IP address of this device (0 for DHCP)
 * @param subnet Subnet mask
 * @param gateway First gateway IP
 */
void DeviceConfiguration::getIPAddress(IPAddress& ip, IPAddress& subnet, IPAddress& gateway)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ip = ip_;
        subnet = subnet_;
        gateway = gateway_;
        xSemaphoreGive(mutexData_);
    }
}

/**
 * Sets IP address to send SNMP traps
 * @param ip IP address to send SNMP traps to
 */
void DeviceConfiguration::setSNMPTrap(const IPAddress& ip)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        snmpTrap_ = ip;
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::SNMP_TRAP_IP);
    }
}

/**
 * Gets IP address to send SNMP traps
 * @param IP address to send SNMP traps to
 */
void DeviceConfiguration::getSNMPTrap(IPAddress& ip)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ip = snmpTrap_;
        xSemaphoreGive(mutexData_);
    }
}

/**
 * Sets the temperature alarm threshold
 * @param alarm Alarm threshold
 */
void DeviceConfiguration::setTemperatureAlarm(double alarm)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        tempAlarm_ = alarm;
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::TEMPERATURE_ALARM);
    }
}

/**
 * Gets temperature alarm threshold
 */
double DeviceConfiguration::getTemperatureAlarm()
{
    double ret = 0.0;
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        ret = tempAlarm_;
        xSemaphoreGive(mutexData_);
    }
    return ret;
}

/**
 * Loop to delay flash writing
 */
void DeviceConfiguration::loop()
{
    unsigned long now  = millis();
    bool doWrite = false;
    if(xSemaphoreTake(mutexData_, 0 ) == pdTRUE)
    {
        if((lastChange_ != 0) && ((now - lastChange_) >= FLASH_SAVE_DELAY)){
            doWrite = true;
            lastChange_ = 0;
        }
        xSemaphoreGive(mutexData_);
    }

    if(doWrite){
        write();
    }
}

/**
 * Create a JSON version of the configuration
 */
void DeviceConfiguration::toJSONString(std::string& str)
{
    JsonDocument doc;
    toJSON(doc);
    serializeJson(doc, str);
}

void DeviceConfiguration::toJSON(JsonDocument& doc)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {        
        doc["DeviceName"] = deviceName_;
        doc["IP"] = ip_ == INADDR_NONE ? "DHCP" : ip_.toString();
        doc["Subnet"] = subnet_ == INADDR_NONE ? "DHCP" : subnet_.toString();
        doc["Gateway"] = subnet_ == INADDR_NONE ? "DHCP" : gateway_.toString();
        doc["TemperatureMax"] = tempAlarm_;        
        xSemaphoreGive(mutexData_);
    }
}

void DeviceConfiguration::registerListener(ParameterListener listener)
{
    if(xSemaphoreTake(mutexListeners_, portMAX_DELAY ) == pdTRUE)
    {
        listeners_.push_back(listener);
        xSemaphoreGive(mutexListeners_);
    }
}

void DeviceConfiguration::notifyListeners(Parameter changed)
{
    if(xSemaphoreTake(mutexListeners_, portMAX_DELAY ) == pdTRUE)
    {
        for(ParameterListener l: listeners_){
            l(changed);
        }
        xSemaphoreGive(mutexListeners_);
    }
}

void DeviceConfiguration::write()
{
    ESP_LOGI(CFGTAG, "Saving configuration to flash");
    File cfgFile = LittleFS.open(CFG_FILENAME, "w");
    JsonDocument doc;
    toJSON(doc);
    serializeJson(doc, cfgFile);
    cfgFile.close();
}