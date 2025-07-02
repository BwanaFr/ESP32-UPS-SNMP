#include <Configuration.hpp>
#include "esp_log.h"
#include <LittleFS.h>

#include <ETH.h>
#include "mbedtls/aes.h"
#include "mbedtls/cipher.h"

#define DEFAULT_TEMPERATURE_ALARM 65.0
#define FLASH_SAVE_DELAY 1000
#define CFG_FILENAME "/config.json"

static const char* TAG = "Configuration";

DeviceConfiguration Configuration;

DeviceConfiguration::DeviceConfiguration() : 
        lastChange_(0), tempAlarm_(DEFAULT_TEMPERATURE_ALARM),
        ip_(INADDR_NONE), subnet_(INADDR_NONE), gateway_(INADDR_NONE)
{
    mutexData_ = xSemaphoreCreateMutex();
    if(mutexData_ == NULL){
        ESP_LOGE(TAG, "Unable to create data mutex");
    }

    mutexListeners_ = xSemaphoreCreateMutex();
    if(mutexListeners_ == NULL){
        ESP_LOGE(TAG, "Unable to create listeners mutex");
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
            //Loads credentials outside fromJSON
            if(json["Username"]){
                setUserName(json["Username"]);
            }

            if(json["Password"]){
                password_ = json["Password"].as<std::string>();
            }
            lastChange_ = 0;    //Don't write to flash
        }
        configFile.close();
    }    
}

void DeviceConfiguration::fromJSON(const JsonDocument& doc)
{
    if(doc["Device_name"]){
        setDeviceName(doc["Device_name"]);
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

    if(doc["Temperature_max"]){
        setTemperatureAlarm(doc["Temperature_max"]);
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

void DeviceConfiguration::toJSON(JsonDocument& doc, bool includeLogin)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {        
        doc["Device_name"] = deviceName_;
        doc["IP"] = ip_ == INADDR_NONE ? "DHCP" : ip_.toString();
        doc["Subnet"] = subnet_ == INADDR_NONE ? "DHCP" : subnet_.toString();
        doc["Gateway"] = subnet_ == INADDR_NONE ? "DHCP" : gateway_.toString();
        doc["Temperature_max"] = tempAlarm_;
        if(includeLogin){
            doc["Username"] = userName_;
            doc["Password"] = password_;
        }
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
    ESP_LOGI(TAG, "Saving configuration to flash");
    File cfgFile = LittleFS.open(CFG_FILENAME, "w");
    JsonDocument doc;
    toJSON(doc, true);
    serializeJson(doc, cfgFile);
    cfgFile.close();
}

void DeviceConfiguration::generateKeys(unsigned char iv[16], unsigned char key[128])
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    ETH.macAddress(mac);
    for(int i=0;i<16;++i){
        iv[i] = mac[i%6];
    }
    for(int i=0;i<128;++i){
        key[i] = mac[i%6] + (mac[0] << 3);
    }
}

std::string DeviceConfiguration::encrypt(const std::string& input)
{
    unsigned char* out;
    std::string inData = input;
    unsigned long cipherLen = inData.size() + 16 - (inData.size() % 16);
    inData.resize(cipherLen);
    out = (unsigned char*)malloc(cipherLen);
    if(!out){
        ESP_LOGE(TAG, "Unable to allocate crypto buffer!");
    }
    unsigned char iv[16];
    unsigned char key[128];
    generateKeys(iv, key);
	mbedtls_aes_context aes;
	mbedtls_aes_init(&aes);
	mbedtls_aes_setkey_enc(&aes, key, 128);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, cipherLen, iv, (const unsigned char*)input.c_str(), out);
	mbedtls_aes_free(&aes);

    std::string ret;
    ret.resize(cipherLen*2);
    for(int i=0;i<cipherLen;++i){
        sprintf(&ret[i*2], "%02X", out[i]);        
    }
    return ret;
}

std::string DeviceConfiguration::decrypt(const std::string& input)
{
    std::string ret;
    size_t dataSize = input.length()/2;
    ret.resize(dataSize);
    unsigned char* in;
    unsigned char iv[16];
    unsigned char key[128];
    generateKeys(iv, key);

    in = (unsigned char*)malloc(dataSize);
    if(!in){
        ESP_LOGE(TAG, "Unable to allocate crypto buffer! (%u bytes)", dataSize);
    }else{
        for(int i=0;i<dataSize;++i){
            char data[3] = {input[i*2], input[i*2+1], '\0'};
            sscanf(data, "%02X", &in[i]);
        }
    }
    mbedtls_aes_context aes;
	mbedtls_aes_init(&aes);
	mbedtls_aes_setkey_enc(&aes, key, 128);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, dataSize, iv, in, (unsigned char*)&ret[0]);
	mbedtls_aes_free(&aes);
    return ret;
}

void DeviceConfiguration::setUserName(const std::string& user)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        userName_ = user;
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::LOGIN_USER);
    }
}

/**
 * Gets configuration user name
 */
void DeviceConfiguration::getUserName(std::string& user)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        user = userName_;
        xSemaphoreGive(mutexData_);
    }
}

/**
 * Sets configuration password
 */
void DeviceConfiguration::setPassword(const std::string& password)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        password_ = encrypt(password);
        lastChange_ = millis();
        xSemaphoreGive(mutexData_);
        notifyListeners(Parameter::LOGIN_PASS);
    }
}

/**
 * Gets configuraton password
 */
void DeviceConfiguration::getPassword(std::string& password)
{
    if(xSemaphoreTake(mutexData_, portMAX_DELAY ) == pdTRUE)
    {
        if(password_.empty()){
            password = "";
        }else{
            password = decrypt(password_);
        }
        xSemaphoreGive(mutexData_);
    }
}