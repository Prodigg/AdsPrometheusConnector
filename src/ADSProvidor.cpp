//
// Created by prodigg on 04.02.26.
//
#include "ADSProvidor.h"
#include "AdsLib.h"
#include "AdsDevice.h"

#include <iostream>
#include <mutex>

#include "AdsVariableList.h"

AdsProvider_t::AdsProvider_t(ProcessDataBuffer_t& processDataBuffer, AmsNetId remoteAmsNetId, std::string remoteIPv4, const AmsNetId localAmsNetId, const long refreshTimeResolution) :
    _processDataBuffer(processDataBuffer), _refreshTimeResolution(refreshTimeResolution) {

    bhf::ads::SetLocalAddress(localAmsNetId);
    _device.emplace(remoteIPv4, remoteAmsNetId, AMSPORT_R0_PLC_TC3); //TODO: make port configurable in config
    std::cout << "INFO: Starting ADS client. Remote IP: " << remoteIPv4 << " Remote AmsNetID: " << remoteAmsNetId << " Port: " << AMSPORT_R0_PLC_TC3 << "\n";
    _thread.emplace(std::jthread(&AdsProvider_t::threadLoop, this));
}

AdsProvider_t::~AdsProvider_t() {
    _thread.reset();
    _device.reset();
}

void AdsProvider_t::addSymbol(const std::string& symbolName, symbolDataType_t symbolType, std::chrono::high_resolution_clock::duration scrapingTime) {
    std::scoped_lock(_symbolNamesMutex);
    _symbolNames.emplace_back(symbolName, symbolType, scrapingTime, std::chrono::high_resolution_clock::now());
}

void AdsProvider_t::updateSymbolProcessDataBufferFailed(symbolDefinition_t& symbolDefinition) const {
    symbolData_t data;
    _processDataBuffer.getSymbolData(symbolDefinition.symbolName, data);
    data.wasLastReadSuccessful = false;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName, data);
    symbolDefinition.lastRead = std::chrono::high_resolution_clock::now();
}

void AdsProvider_t::updateSymbolProcessDataBufferFailed(const std::string & symbolName) {
    size_t symbolDefinitionIndex = -1;
    for (size_t i = 0; i < _symbolNames.size(); ++i) {
        if (symbolName== _symbolNames.at(i).symbolName) {
            symbolDefinitionIndex = i;
            break;
        }
    }
    if (symbolDefinitionIndex == -1) {
        std::cerr << "Error couldn't resolve Ads Symbol " << symbolName << std::endl;
        return;
    }
    updateSymbolProcessDataBufferFailed(_symbolNames.at(symbolDefinitionIndex));
}

void AdsProvider_t::updateSymbolProcessDataBuffer(symbolDefinition_t& symbolDefinition, const std::string& value, const std::chrono::steady_clock::time_point readStartTime) const {
    const auto lastCurrentTime = std::chrono::high_resolution_clock::now() - symbolDefinition.lastRead;
    const auto symbolReadTime = std::chrono::steady_clock::now() - readStartTime;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName,
        {.lastCurrentTime = lastCurrentTime,
            .symbolReadTime = symbolReadTime,
            .lastReadTime = std::chrono::system_clock::now(),
            .wasLastReadSuccessful = true,
            .symbolValue = value
        });
    symbolDefinition.lastRead = std::chrono::high_resolution_clock::now();
}

void AdsProvider_t::threadLoop(std::stop_token stoken) {
    auto next = std::chrono::steady_clock::now();

    while (!stoken.stop_requested()) {
        {
            std::scoped_lock(_symbolNamesMutex);
            readSymbols();
        }
        next += std::chrono::milliseconds(_refreshTimeResolution);
        std::this_thread::sleep_until(next); // wait to allow for addSymbol to insert data into _symbolName
    }
}

void AdsProvider_t::readSymbols() {
    for (symbolDefinition_t& symbol: _symbolNames) {
        if (symbol.lastRead + symbol.expirationDuration <= std::chrono::high_resolution_clock::now())
            addSymbolForReading(symbol);
    }
    readAllMarkedSymbols();
}

void AdsProvider_t::addSymbolForReading(symbolDefinition_t& symbolDefinition) {
    _symbolsToRead.push(&symbolDefinition);
}

void AdsProvider_t::readAllMarkedSymbols() {
    if (_symbolsToRead.empty())
        return;

    const auto startReadTime = std::chrono::steady_clock::now();

    std::vector<std::string> symbolsToRead;
    if (_symbolsToRead.size() > 200)
        symbolsToRead.reserve(200);
    else
        symbolsToRead.reserve(_symbolsToRead.size());

    // read symbols until empty
    size_t readGroupIndex = 0;
    while (!_symbolsToRead.empty()) {
        for (int i = 0; i < symbolsToRead.capacity(); ++i) {
            symbolsToRead.emplace_back(_symbolsToRead.top()->symbolName);
            _symbolsToRead.pop();
        }

        AdsVariableList readVars(_device.value(), symbolsToRead, _symbolCache);
        try {
            readVars.read();
        } catch (const AdsException& e) {
            std::cerr << "Error while reading Ads Symbols " << ": " << e.what() << std::endl;
            for (const std::string & symbol: symbolsToRead) {
                updateSymbolProcessDataBufferFailed(symbol);
            }
        }

        // insert data into process data buffer
        for (const std::string & symbolName: symbolsToRead) {
            updateSymbolProcessDataBuffer(symbolName, readVars, startReadTime);
        }

        _processDataBuffer.insertReadGroupMetric({
            .readTime = std::chrono::steady_clock::now() - startReadTime,
            .dataReadTime = std::chrono::system_clock::now(),
            .worker = "main",
            .readGroup = std::to_string(readGroupIndex)});

        symbolsToRead.clear();
        ++readGroupIndex;
    }
}

void AdsProvider_t::updateSymbolProcessDataBuffer(const std::string& symbolName, const AdsVariableList& varList, std::chrono::steady_clock::time_point readStartTime) {
    // search for symbol definition
    size_t symbolDefinitionIndex = -1;
    for (size_t i = 0; i < _symbolNames.size(); ++i) {
        if (symbolName== _symbolNames.at(i).symbolName) {
            symbolDefinitionIndex = i;
            break;
        }
    }
    if (symbolDefinitionIndex == -1) {
        std::cerr << "Error couldn't resolve Ads Symbol " << symbolName << std::endl;
        return;
    }

    symbolDefinition_t& symbolDefinition = _symbolNames.at(symbolDefinitionIndex);

    try {
        if (symbolDefinition.symbolType == symbolDataType_t::e_bool) {
            const auto* data = static_cast<bool *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(_symbolNames.at(symbolDefinitionIndex), *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_char) {
            const auto* data = static_cast<char *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_double) {
            const auto* data = static_cast<double *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_float) {
            const auto* data = static_cast<float *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int8_t) {
            const auto* data = static_cast<int8_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int16_t) {
            const auto* data = static_cast<int16_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int32_t) {
            const auto* data = static_cast<int32_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int64_t) {
            const auto* data = static_cast<int64_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_string) {
            //const AdsVariable<std::string> readVarString{ _device.value(), symbolDefinition.symbolName };
            //updateSymbolProcessDataBuffer(symbolDefinition, readVarString, readStartTime);$
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint8_t) {
            const auto* data = static_cast<uint8_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint16_t) {
            const auto* data = static_cast<uint16_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint32_t) {
            const auto* data = static_cast<uint32_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint64_t) {
            const auto* data = static_cast<uint64_t *>(varList.getSymbolData(symbolName, nullptr));
            updateSymbolProcessDataBuffer(symbolDefinition, *data, readStartTime);
        } else
            throw std::runtime_error("unknown symbol type");
    } catch (const AdsException& e) {
        std::cerr << "Error while reading Ads Symbol " << symbolDefinition.symbolName << ": " << e.what() << std::endl;
        updateSymbolProcessDataBufferFailed(symbolDefinition);
    }
}

uint32_t AdsProvider_t::mapSymbolTipeToSize(const symbolDataType_t& symbolType ) {
    switch (symbolType) {
        case symbolDataType_t::e_bool:
            return sizeof(bool);
        case symbolDataType_t::e_char:
            return sizeof(char);
        case symbolDataType_t::e_double:
            return sizeof(double);
        case symbolDataType_t::e_float:
            return sizeof(float);
        case symbolDataType_t::e_int8_t:
            return sizeof(int8_t);
        case symbolDataType_t::e_int16_t:
            return sizeof(int16_t);
        case symbolDataType_t::e_int32_t:
            return sizeof(int32_t);
        case symbolDataType_t::e_int64_t:
            return sizeof(int64_t);
        case symbolDataType_t::e_string:
            throw std::runtime_error("string is not supported");
        case symbolDataType_t::e_uint8_t:
            return sizeof(uint8_t);
        case symbolDataType_t::e_uint16_t:
            return sizeof(uint16_t);
        case symbolDataType_t::e_uint32_t:
            return sizeof(uint32_t);
        case symbolDataType_t::e_uint64_t:
            return sizeof(uint64_t);
        default:
            throw std::runtime_error("unknown symbol type");
    }
}

uint32_t AdsProvider_t::durationToNs(std::chrono::high_resolution_clock::duration duration) {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 100);
}
