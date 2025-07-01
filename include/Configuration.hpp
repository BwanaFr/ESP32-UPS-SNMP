#ifndef _CONFIGURATION_HPP__
#define _CONFIGURATION_HPP__

#include <string>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include <FreeRTOS.h>

class DeviceConfiguration {
public:
    /**
     * Enumeration of parameter
     */
    enum class Parameter {
        DEVICE_NAME = 0,
        IP_CONFIGURATION,
        SNMP_TRAP_IP,
        TEMPERATURE_ALARM
    };

    DeviceConfiguration();
    virtual ~DeviceConfiguration() = default;

    /**
     * Setup
     */
    void begin();

    /**
     * Loads configuration from JSON file in flash
     */
    void load();

    /**
     * Sets the name of the device
     * @param name Name of the device
     */
    void setDeviceName(const std::string& name);

    /**
     * Gets device name
     * @param name Name of the device
     */
    void getDeviceName(std::string& name);

    /**
     * Sets static IP address
     * @param ip IP Address of this device (0 to enable DHCP)
     * @param subnet IP subnet mask
     * @param gateway First gateway IP
     */
    void setIPAddress(const IPAddress& ip, const IPAddress& subnet, const IPAddress& gateway);

    /**
     * Gets IP address of this device
     * @param ip IP address of this device (0 for DHCP)
     * @param subnet Subnet mask
     * @param gateway First gateway IP
     */
    void getIPAddress(IPAddress& ip, IPAddress& subnet, IPAddress& gateway);

    /**
     * Sets IP address to send SNMP traps
     * @param ip IP address to send SNMP traps to
     */
    void setSNMPTrap(const IPAddress& ip);

    /**
     * Gets IP address to send SNMP traps
     * @param IP address to send SNMP traps to
     */
    void getSNMPTrap(IPAddress& ip);

    /**
     * Sets the temperature alarm threshold
     * @param alarm Alarm threshold
     */
    void setTemperatureAlarm(double alarm);

    /**
     * Gets temperature alarm threshold
     */
    double getTemperatureAlarm();

    /**
     * Loop to delay flash writing
     */
    void loop();

    /**
     * Create a JSON string of the configuration
     */
    void toJSONString(std::string& str);

    /**
     * Fill configuration to JSON document
     */
    void toJSON(JsonDocument& doc);

    /**
     * Loads from JSON document
     */
    void fromJSON(const JsonDocument& doc);

    /**
     * Paramter change callback
     */
    typedef std::function<void(Parameter)> ParameterListener;

    /**
     * Register a new listener
     */
    void registerListener(ParameterListener listener);

private:
    unsigned long lastChange_;                  //!< Last change of one setting
    double tempAlarm_;                          //!< Temperature alarm
    SemaphoreHandle_t mutexData_;               //!< Protect access to ressources
    SemaphoreHandle_t mutexListeners_;          //!< Protect access to listeners vector
    std::string deviceName_;                    //!< Device name
    IPAddress ip_;                              //!< Device IP (0 for DHCP)
    IPAddress subnet_;                          //!< Device Subnet if static IP
    IPAddress gateway_;                         //!< Next gateway if static IP
    IPAddress snmpTrap_;                        //!< SNMP trap IP address
    std::vector<ParameterListener> listeners_;  //!< Configuration listeners_

    void notifyListeners(Parameter changed);
    void write();
};

extern DeviceConfiguration Configuration;

#endif