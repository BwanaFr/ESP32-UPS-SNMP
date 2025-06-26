#include <UPSHIDDevice.hpp>
#include "esp_log.h"
#include <limits>

static const char *TAG_UPS = "UPSHID";

HIDData::HIDData(uint8_t usagePage, uint8_t usage, const char* name) : 
    usagePage_(usagePage), usage_(usage), name_(name), used_(false),
    reportId_(0)
{
}
    
bool HIDData::match(uint8_t usagePage, uint8_t usage)
{
    return (usagePage_ == usagePage) && (usage == usage_);
}

int32_t HIDData::getValue(const uint8_t* buffer, size_t len)
{
    int32_t ret = 0;
    //TODO: Maybe we don't need to recompute this every time
    int32_t physicalMin = 0;
    int32_t physicalMax = 0;
    int32_t unitExponent = 0;
    if((!physicalMaximum_) || (!physicalMinimum_) || ((physicalMaximum_.getValue() == 0) && (physicalMinimum_.getValue() == 0))){
        physicalMin = logicalMinimum_.getValue();
        physicalMax = logicalMaximum_.getValue();
    }
    if(unitExponent_){
        unitExponent = unitExponent_.getValue();
    }
    double resolution = (logicalMaximum_.getValue() - logicalMinimum_.getValue())/
        ((physicalMax - physicalMin) * pow(10.0, unitExponent));

    //TODO: Handle signed/unsigned (page 38 of HID 1.11)
    uint8_t byteNumber = bitPlace_ / 8;
    uint8_t startBit = bitPlace_ - (byteNumber*8);
    uint32_t bitMask = (1<<bitWidth_)-1;
    
    for(int i=0;i<bitWidth_;++i){
        ret |= ((buffer[byteNumber] >> startBit) & 0x1) << i;
        ++startBit;
        if(startBit >= 8){
            startBit = 0;
            ++byteNumber;
        }
    }
    
    ESP_LOGI(TAG_UPS, "Byte number : %u, mask : 0x%0x, resolution : %f, value: 0x%X", byteNumber, bitMask, resolution, ret);
    return ret;
}

UPSHIDDevice::UPSHIDDevice() : 
    datas_{
        //List what can be interresting
        HIDData(BATTERY_SYSTEM_PAGE, REMAINING_CAPACITY_USAGE, "Remaining Capacity"),
        HIDData(BATTERY_SYSTEM_PAGE, AC_PRESENT_USAGE, "AC present"),
        HIDData(BATTERY_SYSTEM_PAGE, CHARGING_USAGE, "Charging"),
        HIDData(BATTERY_SYSTEM_PAGE, DISCHARGING_USAGE, "Discharging"),
        HIDData(BATTERY_SYSTEM_PAGE, BATTERY_PRESENT_USAGE, "Battery present"),
        HIDData(BATTERY_SYSTEM_PAGE, NEEDS_REPLACEMENT_USAGE, "Needs replacement"),
        HIDData(BATTERY_SYSTEM_PAGE, RUN_TIME_TO_EMPTY_USAGE, "Run time to empty")
    }
{

}

void UPSHIDDevice::buildFromHIDReport(const uint8_t* data, size_t dataLen)
{
    HIDGlobalItems globalItems;
    HIDLocalItem localItems;
    uint32_t actualBit = 0;
    for(size_t i=0;i<dataLen;++i){
        HIDReportItemPrefix prefix(data[i]);
        updateGlobalItems(globalItems, prefix, &data[i+1]);
        updateLocalItems(localItems, prefix, &data[i+1]);
        if(prefix.bType == HIDReportItemPrefix::BTYPE::Global && prefix.bTag.globalTag == HIDReportItemPrefix::GlobalTag::ReportID){
            actualBit = 0;
        }else if(prefix.bType == HIDReportItemPrefix::BTYPE::Main){
            if(prefix.bTag.mainTag == HIDReportItemPrefix::MainTag::Input){
                for(int j=0;j<sizeof(datas_)/sizeof(HIDData);++j){
                    if(globalItems.usagePage && localItems.usage && datas_[j].match(globalItems.usagePage.getValue(), localItems.usage.getValue()) && !datas_[j].isUsed()){                        
                        datas_[j].setUsed(true);
                        datas_[j].setReportId(globalItems.reportID.getValue());
                        datas_[j].setBitsConfiguration(actualBit, globalItems.reportSize.getValue());
                        datas_[j].setLogicalMaximum(globalItems.logicalMaximum);
                        datas_[j].setLogicalMinimum(globalItems.logicalMinimum);
                        datas_[j].setPhysicalMaximum(globalItems.physicalMaximum);
                        datas_[j].setPhysicalMinimum(globalItems.physicalMinimum);
                        datas_[j].setUnitExponent(globalItems.unitExponent);
                        //TODO: Implement unit
                    }
                }
                actualBit += globalItems.reportCount * globalItems.reportSize;
            }
            localItems.reset();
        }
        //Advance in buffer
        i += prefix.bSize;
    }
}

void UPSHIDDevice::hidReportData(const uint8_t* data, size_t len)
{
    uint8_t reportID = data[0];
    for(int j=0;j<sizeof(datas_)/sizeof(HIDData);++j){
        if(reportID == datas_[j].getReportId()){
            ESP_LOGI(TAG_UPS, "Found report %s on ID 0x%02x", datas_[j].getName(), reportID);
            int32_t value = datas_[j].getValue(&data[1], len-1);
        }
    }
}

void UPSHIDDevice::updateGlobalItems(HIDGlobalItems& store, const HIDReportItemPrefix& prefix, const uint8_t* data)
{
    if(prefix.bType == HIDReportItemPrefix::BTYPE::Global){
        
        switch(prefix.bTag.globalTag){
            case HIDReportItemPrefix::GlobalTag::UsagePage:
                store.usagePage.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::LogicalMin:
                store.logicalMinimum.setValue(toSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::LogicalMax:
                store.logicalMaximum.setValue(toSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::PhysicalMin:
                store.physicalMinimum.setValue(toSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::PhysicalMax:
                store.physicalMaximum.setValue(toSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::UnitExp:
                store.unitExponent.setValue(toSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::Unit:
                store.unit.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::ReportSize:
                store.reportSize.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::GlobalTag::ReportID:
                store.reportID.setValue(data[0]);
                break;
            case HIDReportItemPrefix::GlobalTag::ReportCount:
                store.reportCount.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
        }
    }
}

void UPSHIDDevice::updateLocalItems(HIDLocalItem& store, const HIDReportItemPrefix& prefix, const uint8_t* data)
{
    if(prefix.bType == HIDReportItemPrefix::BTYPE::Local){
        switch(prefix.bTag.localTag){
            case HIDReportItemPrefix::LocalTag::Usage:
                store.usage.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::UsageMin:
                store.usageMinimum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::UsageMax:
                store.usageMaximum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::DesignatorIndex:
                store.designatorIndex.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::DesignatorMin:
                store.designatorMinimum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::DesignatorMax:
                store.designatorMaximum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::StringIndex:
                store.stringIndex.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::StringMin:
                store.stringMinimum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::StringMax:
                store.stringMaximum.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
            case HIDReportItemPrefix::LocalTag::Delimiter:
                store.delimiter.setValue(toUnSignedInteger(data, prefix.bSize));
                break;
        }
    }
}

void UPSHIDDevice::printHIDReportItemPrefix(const HIDReportItemPrefix& item)
{   
    const char* bType = nullptr;
    const char* tagString = nullptr;
    switch(item.bType){
        case HIDReportItemPrefix::BTYPE::Global:
            bType = "Global";
            tagString = globalTagToString(item.bTag.globalTag);
            break;
        case HIDReportItemPrefix::BTYPE::Local:
            bType = "Local";
            tagString = localTagToString(item.bTag.localTag);
            break;
        case HIDReportItemPrefix::BTYPE::Main:
            bType = "Main";
            tagString = mainTagToString(item.bTag.mainTag);
            break;
        default:
            ESP_LOGI(TAG_UPS, "Unsupported bType");
            return;
    };
    if(tagString == nullptr){
        ESP_LOGI(TAG_UPS, "Invalid item : 0x%02x", item.raw);
    }else{
        ESP_LOGI(TAG_UPS, "Item(%s) : %s, data=%d bytes", bType, tagString, item.bSize);
    }
}

const char* UPSHIDDevice::mainTagToString(HIDReportItemPrefix::MainTag mainTag)
{
    switch (mainTag)
    {
    case HIDReportItemPrefix::MainTag::Input:
        return "Input";
    case HIDReportItemPrefix::MainTag::Output:
        return "Output";
    case HIDReportItemPrefix::MainTag::Feature:
        return "Feature";
    case HIDReportItemPrefix::MainTag::Collection:
        return "Collection";
    case HIDReportItemPrefix::MainTag::EndCollection:
        return "EndCollection";
    }
    return nullptr;
}

const char* UPSHIDDevice::globalTagToString(HIDReportItemPrefix::GlobalTag globalTag)
{
    switch(globalTag)
    {
    case HIDReportItemPrefix::GlobalTag::UsagePage:
        return "UsagePage";
    case HIDReportItemPrefix::GlobalTag::LogicalMin:
        return "Logical minimum";
    case HIDReportItemPrefix::GlobalTag::LogicalMax:
        return "Logical maximum";
    case HIDReportItemPrefix::GlobalTag::PhysicalMin:
        return "Physical minimum";
    case HIDReportItemPrefix::GlobalTag::PhysicalMax:
        return "Physical maximum";
    case HIDReportItemPrefix::GlobalTag::UnitExp:
        return "Unit exponent";
    case HIDReportItemPrefix::GlobalTag::Unit:
        return "Unit";
    case HIDReportItemPrefix::GlobalTag::ReportSize:
        return "Report size";
    case HIDReportItemPrefix::GlobalTag::ReportID:
        return "Report ID";
    case HIDReportItemPrefix::GlobalTag::ReportCount:
        return "Report count";
    case HIDReportItemPrefix::GlobalTag::Push:
        return "Push";
    case HIDReportItemPrefix::GlobalTag::Pop:
        return "Pop";
    }
    return nullptr;
}

const char* UPSHIDDevice::localTagToString(HIDReportItemPrefix::LocalTag localTag)
{
    switch(localTag){
    case HIDReportItemPrefix::LocalTag::Usage:
        return "Usage";
    case HIDReportItemPrefix::LocalTag::UsageMin:
        return "Usage minimum";
    case HIDReportItemPrefix::LocalTag::UsageMax:
        return "Usage maximum";
    case HIDReportItemPrefix::LocalTag::DesignatorIndex:
        return "Designator index";
    case HIDReportItemPrefix::LocalTag::DesignatorMin:
        return "Designator minimum";
    case HIDReportItemPrefix::LocalTag::DesignatorMax:
        return "Designator maximum";
    case HIDReportItemPrefix::LocalTag::StringIndex:
        return "String index";
    case HIDReportItemPrefix::LocalTag::StringMin:
        return "String minimum";
    case HIDReportItemPrefix::LocalTag::StringMax:
        return "String maximum";
    case HIDReportItemPrefix::LocalTag::Delimiter:
        return "Delimiter";
    }
    return nullptr;
}

int32_t UPSHIDDevice::toSignedInteger(const uint8_t* data, size_t len)
{
    int32_t ret = 0;
	int32_t shift = 24;
	for(int i=len-1;i>=0;--i){       
		ret |= data[i] << shift;
		shift -= 8;
	}
    if(len != 4){
	    int32_t divider = ((1<<((4-len)*8))-1);	
	    return ret/divider;
    }
    return ret;
}

uint32_t UPSHIDDevice::toUnSignedInteger(const uint8_t* data, size_t len)
{
    uint32_t ret = 0;
    int32_t shift = 0;
	for(int i=0;i<len;++i){       
		ret |= data[i] << shift;
		shift += 8;
	}
    return ret;
}