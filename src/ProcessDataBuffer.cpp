//
// Created by prodigg on 01.02.26.
//
#include "ProcessDataBuffer.h"

#include <stdexcept>
#include <unistd.h>

void ProcessDataBuffer_t::setSymbolValue(const std::string &value, const std::string &symbolName) {
    setSymbolData(symbolName, {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(0),
        std::chrono::system_clock::now(),
        true,
        value
    });
}

void ProcessDataBuffer_t::getSymbolValue(const std::string &symbolName, std::string &value) {
    symbolData_t data;
    getSymbolData(symbolName, data);
    value = data.symbolValue;
}

void ProcessDataBuffer_t::setSymbolData(const std::string& symbolName, symbolData_t data) {
    std::scoped_lock lock (_dataAccess);
    if (_symbolsValues.contains(symbolName))
        _symbolsValues.at(symbolName) = data;
    else
        _symbolsValues.emplace(symbolName, data);
}

void ProcessDataBuffer_t::getSymbolData(const std::string& symbolName, symbolData_t& data) {
    std::scoped_lock lock (_dataAccess);
    try {
        data = _symbolsValues.at(symbolName);
    } catch (std::out_of_range &ex) {
        data.symbolValue = "NaN";
        data.wasLastReadSuccessful = false;
    }
}

void ProcessDataBuffer_t::insertReadGroupMetric(const ADSReadGroupMetric_t& readGroupMetric) {
    std::scoped_lock lock (_dataAccess);
    if (const auto it = std::ranges::find(_readGroupMetrics, readGroupMetric); it == _readGroupMetrics.end())
        _readGroupMetrics.push_back(readGroupMetric);
    else {
        it->readTime = readGroupMetric.readTime;
        it->dataReadTime = readGroupMetric.dataReadTime;
    }
}
std::vector<ADSReadGroupMetric_t> ProcessDataBuffer_t::dumpReadGroupMetrics() {
    std::scoped_lock lock (_dataAccess);
    return _readGroupMetrics;
}