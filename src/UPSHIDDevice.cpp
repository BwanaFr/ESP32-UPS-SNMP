#include <UPSHIDDevice.hpp>
#include "esp_log.h"
#include <limits>

static const char *TAG_UPS = "UPSHID";


const UPSHIDDevice::InterestItems UPSHIDDevice::interrest[] = {
    {BATTERY_SYSTEM_PAGE, REMAINING_CAPACITY_USAGE},
    {BATTERY_SYSTEM_PAGE, AC_PRESENT_USAGE},
    {BATTERY_SYSTEM_PAGE, CHARGING_USAGE},
    {BATTERY_SYSTEM_PAGE, DISCHARGING_USAGE},
    {BATTERY_SYSTEM_PAGE, BATTERY_PRESENT_USAGE},
    {BATTERY_SYSTEM_PAGE, NEEDS_REPLACEMENT_USAGE},
};

HIDData::HIDData(uint8_t usagePage, uint8_t usage) : 
    usagePage_(usagePage), usage_(usage), used_(false),
    minimum_(std::numeric_limits<int32_t>::min()),
    maximum_(std::numeric_limits<int32_t>::max()),
    reportId_(0)
{
}
    
bool HIDData::match(uint8_t usagePage, uint8_t usage)
{
    return (usagePage_ == usagePage) && (usage == usage_);
}

UPSHIDDevice::UPSHIDDevice()
{

}

UPSHIDDevice* UPSHIDDevice::buildFromHIDReport(const uint8_t* data, size_t dataLen)
{
    HIDGlobalItems globalItems;
    HIDLocalItem localItems;
    for(size_t i=0;i<dataLen;++i){
        HIDReportItemPrefix prefix(data[i]);
        updateGlobalItems(globalItems, prefix, &data[i+1]);
        updateLocalItems(localItems, prefix, &data[i+1]);
        if(prefix.bType == HIDReportItemPrefix::BTYPE::Main){
            for(int j=0;j<sizeof(interrest)/sizeof(InterestItems);++j){
                if(globalItems.usagePage && (interrest[j].usagePage == globalItems.usagePage.getValue()) && 
                    localItems.usage && (localItems.usage.getValue() == interrest[j].usage)){
                    ESP_LOGI(TAG_UPS, "Found usage 0x%02x on reportID 0x%02x", interrest[j].usage, globalItems.reportID.getValue());
                }
            }
            localItems.reset();
        }
        i += prefix.bSize;
    }
    return nullptr;
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