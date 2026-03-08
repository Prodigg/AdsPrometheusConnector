//
// Created by prodigg on 04.02.26.
//
#include "ADSProvidor.h"
#include "AdsLib.h"
#include "AdsDevice.h"

#include <iostream>
#include <mutex>

AdsProvidor_t::AdsProvidor_t(ProcessDataBuffer_t& processDataBuffer, AmsNetId remoteAmsNetId, std::string remoteIPv4, AmsNetId localAmsNetId, long refreshTimeResolution) :
    _processDataBuffer(processDataBuffer), _refreshTimeResolution(refreshTimeResolution) {

    bhf::ads::SetLocalAddress(localAmsNetId);
    _device.emplace(remoteIPv4, remoteAmsNetId, AMSPORT_R0_PLC_TC3);
    _thread.emplace(std::jthread(&AdsProvidor_t::threadLoop, this));
}

AdsProvidor_t::~AdsProvidor_t() {
    _thread.reset();
    _device.reset();
}

void AdsProvidor_t::addSymbol(const std::string& symbolName, symbolDataType_t symbolType, std::chrono::high_resolution_clock::duration scrapingTime) {
    std::scoped_lock(_symbolNamesMutex);
    _symbolNames.emplace_back(symbolName, symbolType, scrapingTime, std::chrono::high_resolution_clock::now());
}

void AdsProvidor_t::updateSymbolProcessDataBufferFailed(symbolDefinition_t& symbolDefinition) const {
    symbolData_t data;
    _processDataBuffer.getSymbolData(symbolDefinition.symbolName, data);
    data.wasLastReadSuccessful = false;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName, data);
    symbolDefinition.lastRead = std::chrono::high_resolution_clock::now();
}

void AdsProvidor_t::updateSymbolProcessDataBuffer(symbolDefinition_t& symbolDefinition, const std::string value, const std::chrono::steady_clock::time_point readStartTime) const {
    auto lastCurrentTime = std::chrono::high_resolution_clock::now() - symbolDefinition.lastRead;
    auto symbolReadTime = std::chrono::steady_clock::now() - readStartTime;
    _processDataBuffer.setSymbolData(symbolDefinition.symbolName,
        {.lastCurrentTime = lastCurrentTime,
            .symbolReadTime = symbolReadTime,
            .lastReadTime = std::chrono::system_clock::now(),
            .wasLastReadSuccessful = true,
            .symbolValue = value
        });
    symbolDefinition.lastRead = std::chrono::high_resolution_clock::now();
}

void AdsProvidor_t::threadLoop(std::stop_token stoken) {
    auto next = std::chrono::steady_clock::now();

    while (!stoken.stop_requested()) {
        {
            std::scoped_lock(_symbolNamesMutex);
            forceReadSymbol();
        }
        next += std::chrono::milliseconds(_refreshTimeResolution);
        std::this_thread::sleep_until(next); // wait to allow for addSymbol to insert data into _symbolName
    }
}

void AdsProvidor_t::forceReadSymbol() {
    for (symbolDefinition_t& symbol: _symbolNames) {
        if (symbol.lastRead + symbol.expirationDuration <= std::chrono::high_resolution_clock::now())
            readSymbol(symbol);
    }
}

void AdsProvidor_t::readSymbol(symbolDefinition_t& symbolDefinition) {
    auto startReadTime = std::chrono::steady_clock::now();
    try {
        if (symbolDefinition.symbolType == symbolDataType_t::e_bool) {
            const AdsVariable<bool> readVarBool{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<bool>(readVarBool), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_char) {
            const AdsVariable<char> readVarChar{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<char>(readVarChar), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_double) {
            const AdsVariable<double> readVarDouble{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<double>(readVarDouble), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_float) {
            const AdsVariable<float> readVarFloat{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<float>(readVarFloat), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int8_t) {
            const AdsVariable<int8_t> readVarInt8{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<int8_t>(readVarInt8), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int16_t) {
            const AdsVariable<int16_t> readVarInt16{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<int16_t>(readVarInt16), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int32_t) {
            const AdsVariable<int32_t> readVarInt32{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<int32_t>(readVarInt32), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_int64_t) {
            const AdsVariable<int64_t> readVarInt64{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<int64_t>(readVarInt64), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_string) {
            const AdsVariable<std::string> readVarString{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, readVarString, startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint8_t) {
            const AdsVariable<uint8_t> readVarUint8{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<uint8_t>(readVarUint8), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint16_t) {
            const AdsVariable<uint16_t> readVarUint16{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<uint16_t>(readVarUint16), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint32_t) {
            const AdsVariable<uint32_t> readVarUint32{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<uint32_t>(readVarUint32), startReadTime);
        } else if (symbolDefinition.symbolType == symbolDataType_t::e_uint64_t) {
            const AdsVariable<uint64_t> readVarUint64{ _device.value(), symbolDefinition.symbolName };
            updateSymbolProcessDataBuffer(symbolDefinition, static_cast<uint64_t>(readVarUint64), startReadTime);
        } else
            throw std::runtime_error("unknown symbol type");
    } catch (const AdsException& e) {
        std::cerr << "Error while reading Ads Symbol " << symbolDefinition.symbolName << ": " << e.what() << std::endl;
        updateSymbolProcessDataBufferFailed(symbolDefinition);
    }
}

void AdsProvidor_t::subscribeSymbols() {
    throw std::runtime_error("not implemented");
}

uint32_t AdsProvidor_t::mapSymbolTipeToSize(const symbolDataType_t& symbolType ) {
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

uint32_t AdsProvidor_t::durationToNs(std::chrono::high_resolution_clock::duration duration) {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 100);
}

void AdsProvidor_t::subscribeSymbol(symbolDefinition_t& symbolDefinition) {
    if (symbolDefinition.subscribed)
        throw std::runtime_error("symbol already subscribed");
    symbolDefinition.subscribed = true;

    const AdsNotificationAttrib attrib = {
        mapSymbolTipeToSize(symbolDefinition.symbolType),
        ADSTRANS_SERVERONCHA,
        durationToNs(symbolDefinition.expirationDuration),
        { 4000000 }
    };

    //AdsNotification notification = {_device.value(), symbolDefinition.symbolName, attrib, &symbolChangedCallback, 0xBEEFDEAD};

    //symbolDefinition.adsNotification.emplace();
}

void AdsProvidor_t::unsubscribeSymbols() {
    throw std::runtime_error("not implemented");
// delete ads notification attrib and AdsNotification
}