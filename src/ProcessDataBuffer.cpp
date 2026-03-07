//
// Created by prodigg on 01.02.26.
//
#include "ProcessDataBuffer.h"

#include <stdexcept>

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
    std::scoped_lock lock (dataAccess);
    if (symbolsValues.contains(symbolName))
        symbolsValues.at(symbolName) = data;
    else
        symbolsValues.emplace(symbolName, data);
}

void ProcessDataBuffer_t::getSymbolData(const std::string& symbolName, symbolData_t& data) {
    std::scoped_lock lock (dataAccess);
    try {
        data = symbolsValues.at(symbolName);
    } catch (std::out_of_range &ex) {
        data.symbolValue = "NaN";
        data.wasLastReadSuccessful = false;
    }
}