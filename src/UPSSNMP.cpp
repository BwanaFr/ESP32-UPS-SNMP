#include <UPSSNMP.hpp>
#include <ETH.h>

UPSSNMPAgent::UPSSNMPAgent() : agent_("public", "private"), started_(false), udp_(nullptr)
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
            agent_.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.5.11", "Hello world!");
        }
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
    }
}
