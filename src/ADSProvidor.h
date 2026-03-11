//
// Created by prodigg on 01.02.26.
//
// ReSharper disable once CppMissingIncludeGuard

#pragma once

#include "ProcessDataBuffer.h"
#include <string>
#include <vector>
#include <chrono>
#include <concepts>
#include <stack>
#include <type_traits>
#include <thread>

#include "AdsLib.h"
#include "AdsDevice.h"
#include "AdsVariable.h"
#include "AdsNotificationOOI.h"
#include "AdsVariableList.h"
#include "ProcessDataBuffer.h"

enum class symbolDataType_t {
    e_uint8_t = 0,
    e_uint16_t,
    e_uint32_t,
    e_uint64_t,
    e_int8_t,
    e_int16_t,
    e_int32_t,
    e_int64_t,
    e_float,
    e_double,
    e_bool,
    e_char,
    e_string
};

struct symbolDefinition_t {
    std::string symbolName;
    symbolDataType_t symbolType;

    // max time allowed to wait for value change (manual read required)
    std::chrono::high_resolution_clock::duration expirationDuration;
    std::chrono::high_resolution_clock::time_point lastRead;

    // true, if value is subscribed to
    bool subscribed = false;
    std::optional<AdsNotification> adsNotification;
};

/*! Due to constraints to the callback function, subscribing is currently not implemented
 * This may change if the performance is terrible, and significant effort on optimizations is required
*/

class AdsProvider_t {
public:
    explicit AdsProvider_t(ProcessDataBuffer_t& processDataBuffer, AmsNetId remoteAmsNetId, std::string remoteIPv4, AmsNetId localAmsNetId, long refreshTimeResolution);
    ~AdsProvider_t();
    void addSymbol(const std::string& symbolName, symbolDataType_t symbolType, std::chrono::high_resolution_clock::duration scrapingTime);

private:

    static void subscribeSymbols();
    static void unsubscribeSymbols();

    /*!
     * @brief subscribes to symbol
     * @param symbolDefinition symbol to subscribe to
     */
    void subscribeSymbol(symbolDefinition_t& symbolDefinition);

    /*!
     * @brief get from symboltype the size
     * @param symbolType type of symbol
     * @return size of symbol
     */
    static uint32_t mapSymbolTipeToSize(const symbolDataType_t& symbolType );

    /*!
     * @brief uses a symbol definition to set the symbol to a new value
     */
    void updateSymbolProcessDataBuffer(symbolDefinition_t& symbolDefinition, const std::string& value, std::chrono::steady_clock::time_point readStartTime) const;
    void updateSymbolProcessDataBuffer(symbolDefinition_t &symbolDefinition, const char value, const std::chrono::steady_clock::time_point readStartTime) const { updateSymbolProcessDataBuffer (symbolDefinition, std::string{value}, readStartTime);}
    void updateSymbolProcessDataBuffer(symbolDefinition_t& symbolDefinition, const bool value, const std::chrono::steady_clock::time_point readStartTime) const { updateSymbolProcessDataBuffer (symbolDefinition, (value ? std::string("1") : std::string("0")), readStartTime); }
    template <typename T>
    void updateSymbolProcessDataBuffer(symbolDefinition_t& symbolDefinition, const T value, const std::chrono::steady_clock::time_point readStartTime) requires std::is_integral_v<T> || std::is_floating_point_v<T> { updateSymbolProcessDataBuffer (symbolDefinition, std::to_string(value), readStartTime); }

    /*!
     * @brief a special implementation for updateSymbolProcessDataBuffer because it also resolves the datatype
     */
    void updateSymbolProcessDataBuffer(const std::string& symbolName, const AdsVariableList& varList, std::chrono::steady_clock::time_point readStartTime);

    /*!
     * @brief updates the status, if the read fails
     */
    void updateSymbolProcessDataBufferFailed(symbolDefinition_t& symbolDefinition) const;
    void updateSymbolProcessDataBufferFailed(const std::string & symbolName);

    /*!
     * @brief this function checks time last read from symbols and reads them if necessary.
     */
    void forceReadSymbol();

    /*!
     * @brief mark symbol to read
     * @param symbolDefinition
     */
    void readSymbol(symbolDefinition_t& symbolDefinition);

    /*!
     * @brief read all marked symbols and insert them into the process data buffer
     */
    void readAllSymbols();

    /*!
     * Converts a duration to a time that is n*100ns
     * @param duration duration to convert
     * @return *100ns
     */
    static uint32_t durationToNs(std::chrono::high_resolution_clock::duration duration);

    /*!
     * @brief this is the main thread function
     */
    void threadLoop(std::stop_token stoken);

    //! initialize object after calling bhf::ads::SetLocalAddress in the constructor.
    //! expect this variable to ALWAYS have a device.
    std::optional<AdsDevice> _device;

    //! this object should be created in the constructor and destroyed with the destructor
    std::optional<std::jthread> _thread;

    //! this stack points to the data in the vector _symbolNames
    std::stack<symbolDefinition_t*> _symbolsToRead;

    ProcessDataBuffer_t& _processDataBuffer;

    std::atomic<long> _refreshTimeResolution = 500;
    std::mutex _symbolNamesMutex;
    std::vector<symbolDefinition_t> _symbolNames;
    std::unordered_map<std::string, AdsSymbolEntry> _symbolCache;
};