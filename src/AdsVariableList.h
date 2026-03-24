//
// Created by prodigg on 10.03.26.
//

#ifndef CPPADSPROMETHEUSCONNECTOR_ADSVARIABLELIST_H
#define CPPADSPROMETHEUSCONNECTOR_ADSVARIABLELIST_H

#include <cstring>

#include "AdsDevice.h"
#include "vector"
#include <algorithm>
#include <iostream>

/*
 * Thanks to li317546360 for writing this code. GitHub: https://github.com/li317546360
 */

struct cacheSymbolEntry_t {
    AdsSymbolEntry symbolInfo;
    std::string symbolName;
};

struct AdsVariableList {
    AdsVariableList(const AdsDevice& route, const std::vector<std::string>& symbolNames, std::unordered_map<std::string, AdsSymbolEntry>& symbolEntryCache) :
        m_route{route},
        m_symbolNames {symbolNames}
    {
        unsigned int dataSize = 0;
        for (const std::string& symbol: symbolNames) {
            if (symbolEntryCache.contains(symbol)) {
                m_symbolEntries.push_back(symbolEntryCache.at(symbol));
            } else {
                try {
                    AdsSymbolEntry symbolEntry = m_route.getSymbolEntry(symbol);
                    symbolEntryCache.emplace(symbol, symbolEntry);
                    m_symbolEntries.push_back(symbolEntry);
                } catch(const AdsException& e) {
                    std::cerr << "ERROR: during resolving of index/offset of symbol " << symbol << ": " << e.what() << std::endl;
                    continue;
                }
            }
            const AdsSymbolEntry& lastElement = m_symbolEntries.back();
            dataSize += lastElement.size;
            m_symbolInfos.emplace_back(lastElement.iGroup,lastElement.iOffs,lastElement.size);
        }

        m_rBuf.resize(4 * m_symbolNames.size() + dataSize,0);
        m_wBuf.resize(12 * symbolNames.size() + dataSize,0);
        initWBuf();
    }

    void read() {
        _dirtyADSCom = false;
        uint32_t bytesRead = 0;
        const size_t symbolSize = m_symbolNames.size();
        const uint32_t error = m_route.ReadWriteReqEx2(
            ADSIGRP_SUMUP_READ,
            symbolSize,
            m_rBuf.size(),
            const_cast<uint8_t*>(m_rBuf.data()),
            12 * symbolSize,
            m_symbolInfos.data(),
            &bytesRead);
        if (error || (m_rBuf.size() != bytesRead)) {
            _dirtyADSCom = true;
            throw AdsException(error);
        }
    }

    void write() const {
        uint32_t bytesRead = 0;
        const size_t symbolSize = m_symbolNames.size();
        copyData2WriteBuf();

        size_t readSize = 0;
        const auto rbf = getStateBuf(&readSize);

        const uint32_t error = m_route.ReadWriteReqEx2(
            ADSIGRP_SUMUP_WRITE,
            symbolSize,
            readSize,
            rbf,
            m_wBuf.size(),
            m_wBuf.data(),
            &bytesRead);
        if (error || (readSize != bytesRead)) {
            throw AdsException(error);
        }
    }

    [[nodiscard]] size_t getSymbolSize(const std::string& name) const {
        for (size_t i = 0; i < m_symbolNames.size(); ++i) {
            if (name == m_symbolNames.at(i))
                return m_symbolEntries.at(i).size;
        }
        return 0;
    }

    void * getSymbolData(const std::string& name, size_t* length) const {

        auto pdbf = getDataBuf(nullptr);

        if (_dirtyADSCom) {
            // use different way of finding data

            size_t index = 0;
            for (const auto& symbol: m_symbolNames) {
                if (symbol == name)
                    break;
                index++;
            }

            for (int i=0; i<m_symbolInfos.size();++i) {
                if (m_symbolEntries.at(i).iGroup == m_symbolInfos.at(index).indexGroup && m_symbolEntries.at(i).iOffs == m_symbolInfos.at(index).indexOffset) {
                    if (length) {
                        *length = m_symbolEntries.at(i).size;
                    }
                    return pdbf;
                }
                pdbf += m_symbolEntries.at(i).size;
            }
            return nullptr;
        }

        for (int i=0; i<m_symbolEntries.size();++i) {
            if (name == m_symbolNames.at(i)) {
                if (length) {
                    *length = m_symbolEntries.at(i).size;
                }
                return pdbf;
            }
            pdbf += m_symbolEntries.at(i).size;
        }
        return nullptr;
    }

    void setSymbolData(const std::string& name, const void *data, const size_t length, const size_t begin = 0) const {
        size_t size = 0;
        const auto buf = static_cast<uint8_t*>(getSymbolData(name,&size));
        if ((begin + length) > size)
            return;
        std::memcpy((buf + begin),data,length);
    }

    [[nodiscard]] int32_t getStateCode(const std::string& name) const {

        const auto psbf = getStateBuf(nullptr);
        for(int i=0;i<m_symbolNames.size();++i) {
            if (name == m_symbolNames.at(i)) {
                return *reinterpret_cast<int32_t*>(psbf + 4 * i);
            }
        }
        return 0;
    }

private:
    uint8_t * getStateBuf(size_t* size) const{
        const size_t len = 4 * m_symbolNames.size();

        if (size) {
            *size = len;
        }
        return const_cast<uint8_t*>(m_rBuf.data());
    }

    uint8_t * getDataBuf(size_t* size) const{
        const size_t stlen = 4 * m_symbolNames.size();
        const size_t len = m_rBuf.size() - stlen;

        if (size) {
            *size = len;
        }
        return const_cast<uint8_t*>(m_rBuf.data()) + stlen;
    }

    uint8_t * getWriteDataBuf(size_t* size) const{
        const size_t inflen = 12 * m_symbolNames.size();
        const size_t len = m_rBuf.size() - inflen;

        if (size) {
            *size = len;
        }
        return const_cast<uint8_t*>(m_wBuf.data()) + inflen;
    }

    void copyData2WriteBuf() const {
        size_t rdataSize = 0;
        const auto rbf = getDataBuf(&rdataSize);
        const auto wbf = getWriteDataBuf(nullptr);
        std::memcpy(wbf,rbf,rdataSize);
    }

    void initWBuf() {
        auto pInt32buf = reinterpret_cast<int32_t*>(const_cast<uint8_t*>(m_wBuf.data()));
        const auto symbolSize = std::min( {m_symbolNames.size(), m_symbolEntries.size()});
        for (int i = 0; i < symbolSize; ++i) {
            *pInt32buf = m_symbolEntries.at(i).iGroup;
            ++pInt32buf;
            *pInt32buf = m_symbolEntries.at(i).iOffs;
            ++pInt32buf;
            *pInt32buf = m_symbolEntries.at(i).size;
            ++pInt32buf;
        }
    }

    const AdsDevice &m_route;
    const std::vector<std::string> m_symbolNames;
    std::vector<AdsSymbolEntry> m_symbolEntries;
    std::vector<AdsSymbolInfoByName> m_symbolInfos;
    std::vector<uint8_t> m_rBuf;
    std::vector<uint8_t> m_wBuf;

    // if true, something went wrong during ads communication, need to use special matching to get data
    bool _dirtyADSCom = false;
};

#endif //CPPADSPROMETHEUSCONNECTOR_ADSVARIABLELIST_H