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
    std::chrono::steady_clock::duration symbolReadTime; // Time to read the symbol TODO: expected removal later when single symbol read times are deprecated
    std::chrono::system_clock::time_point lastReadTime;
    bool wasLastReadSuccessful = false; // true if last read attempt was successfull
    std::string symbolValue;
};

struct ADSReadGroupMetric_t {
    std::chrono::steady_clock::duration readTime; // the ADS read time
    std::chrono::system_clock::time_point dataReadTime; // for the timestamp
    std::string worker;
    std::string readGroup;
    bool operator==(const ADSReadGroupMetric_t & other) const {
        return worker == other.worker && readGroup == other.readGroup;
    }
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

    /*!
     * @brief insert a readGroupMetric
     * @param readGroupMetric readGroupMetric to insert
     */
    void insertReadGroupMetric(const ADSReadGroupMetric_t& readGroupMetric);

    /*!
     * @brief dumps the read group metrics
     * @return the read group metrics
     */
    [[nodiscard]] std::vector<ADSReadGroupMetric_t> dumpReadGroupMetrics();
private:
    std::mutex _dataAccess;
    std::unordered_map<std::string, symbolData_t> _symbolsValues;
    std::vector<ADSReadGroupMetric_t> _readGroupMetrics;
};
