#ifndef _UPS_SNMP_AGENT_HPP__
#define _UPS_SNMP_AGENT_HPP__
#include <Arduino.h>
#include <SNMPTrap.h>
#include <SNMP_Agent.h>
#include <ETH.h>
#include <vector>

class UPSSNMPAgent
{
public:
    UPSSNMPAgent();
    virtual ~UPSSNMPAgent() = default;
    void begin();
    void start();
    void stop();
    void loop();
private:
    void initializeOID();
    void destroyOID();

    SNMPAgent agent_;    
    bool started_;
    WiFiUDP* udp_;
    unsigned long lastPoll_;
    bool wasConnected_;
    std::vector<ValueCallback*> callbacks_;
    TimestampCallback* timestampCallback_;
    SNMPTrap* upsTrap_;
};
#endif