#ifndef __UPS_HID_DEVICE_HPP__
#define __UPS_HID_DEVICE_HPP__

#include <Arduino.h>
#include <OptionalData.hpp>


class HIDData
{
public:
    HIDData(uint8_t usagePage, uint8_t usage);
    ~HIDData() = default;

    /**
     * Tests if the data match Usage page and usage combination
     */
    bool match(uint8_t usagePage, uint8_t usage);
    
    /**
     * Sets if the data is used
     */
    inline void setUsed(bool used){ used_ = used; };

    /**
     * Gets if the data is used
     */
    inline bool isUsed() const { return used_; };

    /**
     * Sets report Id associated with this data
     */
    inline void setReportId(uint8_t reportId){ reportId_ = reportId; };

    /**
     * Gets associated report ID
     */
    inline uint8_t getReportId() const { return reportId_; };

private:
    uint8_t usagePage_;
    uint8_t usage_;
    int32_t minimum_;
    int32_t maximum_;
    uint8_t reportId_;
    bool used_;
};

class UPSHIDDevice
{
public:

    /**
     * Parse an HID report and create an UPS device
     * @param data Pointer to HID report data
     * @param Lenght of the data pointer
     * @return pointer to the UPSHIDDevice or nullptr if device is not an UPS class
     */
    static UPSHIDDevice* buildFromHIDReport(const uint8_t* data, size_t dataLen);

private:

    /**
     * For storing HID global items
     */
    struct HIDGlobalItems{
        OptionalData<uint32_t> usagePage;         //Current usage page
        OptionalData<int32_t> logicalMinimum;     //Logical minimum value
        OptionalData<int32_t> logicalMaximum;     //Logical maximum value
        OptionalData<int32_t> physicalMinimum;    //Physical minimum value
        OptionalData<int32_t> physicalMaximum;    //Physical maximum value
        OptionalData<int32_t> unitExponent;       //Unit exponent
        OptionalData<uint32_t> unit;              //Unit values
        OptionalData<uint32_t> reportSize;        //Report size in bits
        OptionalData<uint8_t> reportID;           //Report ID
        OptionalData<uint32_t> reportCount;       //Report count (number of data fields for the item)       
    };

    /**
     * Local item
     */
    struct HIDLocalItem {
        OptionalData<uint32_t> usage;
        OptionalData<uint32_t> usageMinimum;
        OptionalData<uint32_t> usageMaximum;
        OptionalData<uint32_t> designatorIndex;
        OptionalData<uint32_t> designatorMinimum;
        OptionalData<uint32_t> designatorMaximum;
        OptionalData<uint32_t> stringIndex;
        OptionalData<uint32_t> stringMinimum;
        OptionalData<uint32_t> stringMaximum;
        OptionalData<uint8_t> delimiter;

        inline void reset() {
            usage.reset();
            usageMinimum.reset();
            usageMaximum.reset();
            designatorIndex.reset();
            designatorMinimum.reset();
            designatorMaximum.reset();
            stringIndex.reset();
            stringMinimum.reset();
            delimiter.reset();
        };
    };

    /**
     * Represents a prefix in HID report
     */
    struct HIDReportItemPrefix {
        uint8_t bSize;
        enum class BTYPE : uint8_t {Main = 0, Global, Local, Reserved};
        BTYPE bType;
        //6.2.2.4 of HID 1.11 spec
        enum class MainTag : uint8_t {Input = 0x8, Output = 0x9, Feature = 0xb, Collection = 0xa, EndCollection = 0xc};
        //6.2.2.7 Of HID 1.11 spec
        enum class GlobalTag : uint8_t {UsagePage = 0x0, LogicalMin, LogicalMax, PhysicalMin, PhysicalMax, UnitExp, Unit, ReportSize, ReportID, ReportCount, Push, Pop};
        //6.2.2.8 of HID 1.11 spec
        enum class LocalTag : uint8_t {Usage=0x0, UsageMin, UsageMax, DesignatorIndex, DesignatorMin, DesignatorMax, StringIndex, StringMin, StringMax, Delimiter};
        union {
            MainTag mainTag;
            GlobalTag globalTag;
            LocalTag localTag;
            uint8_t data;
        } bTag;

        uint8_t raw;
        HIDReportItemPrefix(uint8_t item){
            raw = item;
            bSize = item & 0x3;
            if(bSize == 3){
                bSize = 4;
            }
            bType = static_cast<BTYPE>((item >> 2) & 0x3);
            bTag.data = (item >> 4) & 0xF;
        }
    };

    /**
     * Collection item meaning (6.2.2.6 of HID 1.11 spec)
     */
    enum CollectionItem : uint8_t {Physical = 0x0, Application, Logical, Report, NamedArray, UsageSwitch, UsageModifier};

    
    UPSHIDDevice();
    ~UPSHIDDevice() = default;

    /*Various HID constants taken from
        Universal Serial Bus 
        Usage Tables 
        for                     
        HID Power Devices
    **/
    static constexpr uint8_t BATTERY_SYSTEM_PAGE = 0x85;
    static constexpr uint8_t REMAINING_CAPACITY_USAGE = 0x66;
    static constexpr uint8_t AC_PRESENT_USAGE = 0xd0;
    static constexpr uint8_t CHARGING_USAGE = 0x44;
    static constexpr uint8_t DISCHARGING_USAGE = 0x45;
    static constexpr uint8_t BATTERY_PRESENT_USAGE = 0xd1;
    static constexpr uint8_t NEEDS_REPLACEMENT_USAGE = 0x4b;
    static constexpr uint8_t RUN_TIME_TO_EMPTY_USAGE = 0x68;

    /**
     * Defines what is in interest
     */
    struct InterestItems{
        uint8_t usagePage;
        uint8_t usage;
    };

    static const InterestItems interrest[];

    static void printHIDReportItemPrefix(const HIDReportItemPrefix& item);
    static const char* mainTagToString(HIDReportItemPrefix::MainTag mainTag);
    static const char* globalTagToString(HIDReportItemPrefix::GlobalTag globalTag);
    static const char* localTagToString(HIDReportItemPrefix::LocalTag localTag);

    /**
     * Convert following bytes in the buffer to an unsigned int32_t
     */
    static int32_t toSignedInteger(const uint8_t* data, size_t len);

    static uint32_t toUnSignedInteger(const uint8_t* data, size_t len);

    /**
     * Updates the GlobalItem store
     */
    static void updateGlobalItems(HIDGlobalItems& store, const HIDReportItemPrefix& prefix, const uint8_t* data);

    /**
     * Updates the LocalItem store
     */
    static void updateLocalItems(HIDLocalItem& store, const HIDReportItemPrefix& prefix, const uint8_t* data);
};


#endif