#include <UPSSNMP.hpp>
#include <ETH.h>
#include <UPSHIDDevice.hpp>
#include <Arduino.h>
#include <esp_log.h>
#include <ETH.h>
#include <Configuration.hpp>
#include <Temperature.hpp>

static const char* TAG = "SNMP";

UPSSNMPAgent::UPSSNMPAgent() : agent_("public", "private"), started_(false), udp_(nullptr),
                    lastPoll_(0), wasConnected_(false)
{
}

void UPSSNMPAgent::begin()
{
}

void UPSSNMPAgent::start()
{
    if(!started_){
        if(udp_ == nullptr){
            udp_ = new WiFiUDP();            
            agent_.setUDP(udp_);
            //sysDescr
            agent_.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1", "UPS gateway");
            //sysName
            agent_.addDynamicReadOnlyStringHandler(".1.3.6.1.2.1.1.5", []()->const std::string{ return ETH.getHostname(); });
            //entPhysicalDescr
            agent_.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.47.1.1.1.1.2", "USB UPS to SNMP gateway");
            //entPhysicalName
            agent_.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.47.1.1.1.1.7", "UPS gateway");
            //entPhysicalSerialNum
            agent_.addDynamicReadOnlyStringHandler(".1.3.6.1.2.1.47.1.1.1.1.11", []()->const std::string{ return ETH.macAddress().c_str(); });
            // sysUpTime 
            timestampCallback_ = (TimestampCallback*)agent_.addDynamicReadOnlyTimestampHandler(".1.3.6.1.2.1.1.3", []()->uint32_t{
                return static_cast<uint32_t>(millis()/10);
            });
            //hrSystemUptime
            agent_.addDynamicReadOnlyTimestampHandler(".1.3.6.1.2.1.25.1.1", []()->uint32_t{
                return static_cast<uint32_t>(millis()/10);
            });

            //Temperature
            agent_.addDynamicIntegerHandler(".1.3.6.1.4.1.119.5.1.2.1.5.1", []()->int{
                return tempProbe.getTemperature() * 10.0;
            });

            upsTrap_ = new SNMPTrap("public", SNMP_VERSION_2C);
            upsTrap_->setUDP(udp_); // give a pointer to our UDP object
            upsTrap_->setTrapOID(new OIDType(".1.3.6.1.2.1.33.2")); // OID of the trap
            upsTrap_->setSpecificTrap(1);
            
            // Set the uptime counter to use in the trap (required)
            upsTrap_->setUptimeCallback(timestampCallback_);

            // Set some previously set OID Callbacks to send these values with the trap (optional)

            //upsEstimatedMinutesRemaining 1.3.6.1.2.1.33.1.2.3
            //upsTraps 1.3.6.1.2.1.33.2
            //sysName 1.3.6.1.2.1.1.5
            //1.3.6.1.2.1.1.3 (sysUpTime)
            agent_.sortHandlers();
        }
        if(upsTrap_)
            upsTrap_->setIP(ETH.localIP()); // Set our Source IP
        agent_.begin();
        started_ = true;
    }
}

void UPSSNMPAgent::stop()
{
    if(started_){
        started_ = false;
        agent_.stop();
    }
}

void UPSSNMPAgent::loop()
{
    if(started_){
        agent_.loop();
        //Poll UPS every seconds
        unsigned long now = millis();
        if((now - lastPoll_) > 100){
            bool connected = upsDevice.isConnected();
            if(connected && !wasConnected_){
                ESP_LOGI(TAG, "UPS reconnected!");
                initializeOID();
            }else if(!connected && wasConnected_){
                ESP_LOGI(TAG, "UPS disconnected!");
                destroyOID();
                //TODO: Check SNMP version
                upsTrap_->setVersion(SNMP_VERSION_1);
                upsTrap_->setInform(false);
                IPAddress destinationIP;
                Configuration.getSNMPTrap(destinationIP);
                if(destinationIP != INADDR_NONE){
                    if(agent_.sendTrapTo(upsTrap_, destinationIP, true, 2, 5000) != INVALID_SNMP_REQUEST_ID){ 
                        ESP_LOGI(TAG, "Sent SNMP Trap");
                    }
                }
            }
            wasConnected_ = connected;
        }
    }
}

void UPSSNMPAgent::initializeOID()
{
    //upsEstimatedChargeRemaining OID
    if(upsDevice.getRemainingCapacity().isUsed()){
        callbacks_.push_back(agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.33.1.2.4", []()->int{
            return static_cast<int32_t>(upsDevice.getRemainingCapacity().getValue());
        }));
    }

    //upsEstimatedMinutesRemaining OID
    if(upsDevice.getRuntimeToEmpty().isUsed()){
        callbacks_.push_back(agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.33.1.2.3", []()->int{
            return static_cast<int32_t>(upsDevice.getRuntimeToEmpty().getValue()/60);   //Convert seconds to minutes
        }));
    }
 
    if(upsDevice.getACPresent().isUsed()){
        callbacks_.push_back(agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.33.1.2.4", []()->int{
            return static_cast<int32_t>(upsDevice.getACPresent().getValue());   //Convert seconds to minutes
        }));
    }

    agent_.sortHandlers();
}

void UPSSNMPAgent::destroyOID()
{
    for(ValueCallback* cb : callbacks_){
        agent_.removeHandler(cb);        
    }
    callbacks_.clear();
    agent_.sortHandlers();
}