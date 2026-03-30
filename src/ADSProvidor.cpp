//
// Created by prodigg on 04.02.26.
//
#include "ADSProvidor.h"
#include "AdsLib.h"
#include "AdsDevice.h"

#include <iostream>
#include <mutex>

#include "AdsVariableList.h"

AdsProvider_t::AdsProvider_t(ProcessDataBuffer_t& processDataBuffer, AmsNetId remoteAmsNetId, std::string remoteIPv4, const AmsNetId localAmsNetId, const long refreshTimeResolution, uint16_t amsRemotePort) :
    _processDataBuffer(processDataBuffer), _refreshTimeResolution(refreshTimeResolution) {

    uint16_t _amsRemotePort = AMSPORT_R0_PLC_TC3;
    if (amsRemotePort != 0)
        _amsRemotePort = amsRemotePort;

    bhf::ads::SetLocalAddress(localAmsNetId);
    _device.emplace(remoteIPv4, remoteAmsNetId, AMSPORT_R0_PLC_TC3);
    std::cout << "INFO: Starting ADS client. Remote IP: " << remoteIPv4 <<
        " Remote AmsNetID: " << remoteAmsNetId << " Remote Port: " << _amsRemotePort <<
            " Local AmsNetID: " << localAmsNetId << " RemotePort: " << _amsRemotePort <<
                " LocalPort: " << _device->GetLocalPort() << "\n";

    _thread.emplace(std::jthread(&AdsProvider_t::threadLoop, this));
}

AdsProvider_t::~AdsProvider_t() {
    _thread.reset();
    _device.reset();
}

void AdsProvider_t::addSymbol(const std::string& symbolName, symbolDataType_t symbolType, std::chrono::steady_clock::duration scrapingTime) {
    std::scoped_lock l(_symbolNamesMutex);
    _symbolNames.emplace_back(symbolName, symbolType, scrapingTime, std::chrono::steady_clock::now());
}

void AdsProvider_t::updateSymbolProcessDataBufferFailed(symbolDefinition_t& symbolDefinition) const {
    symbolData_t data;
    _processDataBuffer.getSymbolData(symbolDefinition.symbolName, data);
    data.wasLastReadSuccessful = false;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName, data);
    symbolDefinition.lastRead = std::chrono::steady_clock::now();
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
    const auto lastCurrentTime = std::chrono::steady_clock::now() - symbolDefinition.lastRead;
    const auto symbolReadTime = std::chrono::steady_clock::now() - readStartTime;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName,
        {.lastCurrentTime = lastCurrentTime,
            .symbolReadTime = symbolReadTime,
            .lastReadTime = std::chrono::system_clock::now(),
            .wasLastReadSuccessful = true,
            .symbolValue = value
        });
    symbolDefinition.lastRead = std::chrono::steady_clock::now();
}

void AdsProvider_t::threadLoop(std::stop_token stoken) {
    auto next = std::chrono::steady_clock::now();

    while (!stoken.stop_requested()) {
        {
            std::scoped_lock l(_symbolNamesMutex);
            readGroups();
        }
        next += std::chrono::milliseconds(_refreshTimeResolution);
        std::this_thread::sleep_until(next); // wait to allow for addSymbol to insert data into _symbolName
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
            if (!data) throw std::runtime_error("couldnt get data from symbol");
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
    } catch (const std::exception& e) {
        std::cerr << "Error while reading Ads Symbol " << symbolDefinition.symbolName << ": " << e.what() << std::endl;
        updateSymbolProcessDataBufferFailed(symbolDefinition);
    }
}

uint32_t AdsProvider_t::mapSymbolTypeToSize(const symbolDataType_t& symbolType ) {
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

void AdsProvider_t::readGroups() {
    for (size_t i = 0; i < _readGroups.size(); ++i) {
        ADSReadGroup_t& group = _readGroups.at(i);
        if (group.lastRead + group.scrapingTime <= std::chrono::steady_clock::now())
            readGroup(group, i);
    }
}

void AdsProvider_t::readGroup(ADSReadGroup_t& group, const size_t readGroupIndex) {
    constexpr std::string_view worker ("main");
    if (group.readGroupSymbols.empty())
        return;

    const auto startReadTime = std::chrono::steady_clock::now();
    std::vector<std::string> symbolsToRead; //TODO: fix this redundant copy
    symbolsToRead.reserve(group.readGroupSymbols.size());

    for (const symbolDefinition_t* symbol: group.readGroupSymbols)
        symbolsToRead.emplace_back(symbol->symbolName);

    AdsVariableList readVars(_device.value(), symbolsToRead, _symbolCache);
    try {
        readVars.read();
    } catch (const AdsException& e) {
        std::cerr << "Error while reading Ads Symbols " << ": " << e.what() << " Worker: " << worker << " read Group: " << readGroupIndex << std::endl;
        for (const std::string & symbol: symbolsToRead) {
            updateSymbolProcessDataBufferFailed(symbol);
        }
        // invalidate all symbols of the cache, an error here may be a sign that the address space has been invalidated
        // so an rebuild of the cache is triggered just to be sure
        invalidateAllSymbolInCache();
    }

    // insert data into process data buffer
    for (const std::string & symbolName: symbolsToRead) {
        updateSymbolProcessDataBuffer(symbolName, readVars, startReadTime);
    }

    _processDataBuffer.insertReadGroupMetric({
        .readTime = std::chrono::steady_clock::now() - startReadTime,
        .dataReadTime = std::chrono::system_clock::now(),
        .worker = std::string(worker),
        .readGroup = std::to_string(readGroupIndex),
        .readGroupSymbols = std::move(symbolsToRead),
        .readGroupScrapingTime = group.scrapingTime});

    group.lastRead = std::chrono::steady_clock::now();
}

void AdsProvider_t::generateReadGroups() {
    _readGroups.clear();

    for (symbolDefinition_t & symbol: _symbolNames) {
        // find valid buffer to put value
        const auto groupRule = [&symbol](const ADSReadGroup_t& group) {
            return group.scrapingTime == symbol.expirationDuration && group.readGroupSymbols.size() < 200;
        };

        if (auto it = std::ranges::find_if(_readGroups, groupRule); it != _readGroups.end())
            it->readGroupSymbols.emplace_back(&symbol);
        else {
            auto& group = _readGroups.emplace_back(symbol.expirationDuration, symbol.lastRead);
            group.readGroupSymbols.emplace_back(&symbol);
        }
    }
    std::cout << "INFO: generated " << _readGroups.size() << " read groups from " << _symbolNames.size() << " symbols" << std::endl;
}

void AdsProvider_t::invalidateSymbolInCache(const std::string& symbolName) {
    if (_symbolCache.contains(symbolName))
        _symbolCache.erase(symbolName);
}

void AdsProvider_t::invalidateAllSymbolInCache() {
    _symbolCache.clear();
}