//
// Created by prodigg on 22.01.26.
//

// ReSharper disable once CppMissingIncludeGuard not needed due to using modules
#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <chrono>

struct symbolData_t {
    std::chrono::steady_clock::duration lastCurrentTime; // Time between last and current read
    std::chrono::steady_clock::duration symbolReadTime; // Time to read the symbol
    std::chrono::system_clock::time_point lastReadTime;
    bool wasLastReadSuccessful = false; // true if last read attempt was successfull
    std::string symbolValue;
};

/*!
 * @brief this class is a storage container between ADS receiver and REST endpoint
 * @details it has a mutex to limit the access of the object, the mutex is blocking.
 */
class ProcessDataBuffer_t {
public:
    /*!
     *
     * @param symbolName the symbol name
     * @param data the Data being inserted into the symbol name
     */
    void setSymbolData(const std::string& symbolName, symbolData_t data);

    /*!
     *
     * @param symbolName the symbol name
     * @param data [OUT] the data that is at the symbol name
     */
    void getSymbolData(const std::string& symbolName, symbolData_t& data);

    /*!
     * @param value the value that is inserted in the symbol name
     * @param symbolName the symbol name
     */
    [[deprecated]]
    void setSymbolValue(const std::string &value, const std::string &symbolName);

    /*!
     * @param symbolName the name of the symbol to get
     * @param value [OUT] the value that is at the symbol name
     */
    [[deprecated]]
    void getSymbolValue(const std::string &symbolName, std::string& value);
private:
    std::mutex dataAccess;
    std::unordered_map<std::string, symbolData_t> symbolsValues;
};
