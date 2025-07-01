#ifndef _UPS_SNMP_AGENT_HPP__
#define _UPS_SNMP_AGENT_HPP__
#include <Arduino.h>
#include <SNMPTrap.h>
#include <SNMP_Agent.h>
#include <ETH.h>

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
    SNMPAgent agent_;
    bool started_;
    WiFiUDP* udp_;
};
#endif