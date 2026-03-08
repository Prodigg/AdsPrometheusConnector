//
// Created by prodigg on 24.02.26.
//

#ifndef CPPADSPROMETEUSCONNECTOR_CONFIGURATION_H
#define CPPADSPROMETEUSCONNECTOR_CONFIGURATION_H
#include <cstdint>
#include <string>
#include "ADSProvidor.h"
#include "PrometheusEndpoint.h"
#include "nlohmann/json.hpp"
using json = nlohmann::json;
/*
 * Config formate:
 * "global": {
 *      "localNetId": "<localNetID>",
 *      "remoteNetId": "<remoteNetID>",
 *      "remoteIPv4": "<remoteIPv4>",
 *      "httpPort": <httpPort>,
 *      [optional] "pollTimeResolution": <resolutionTime_ms>
 * },
 *
 *
 * "<symbolADSName>": {
 * [optional] "alias": "<aliasName>",
 * [optional] "type": "<untyped|counter|gauge>",
 * [optional] "description": "<description>",
 * "ADSDatatype": "<datatypes>",
 * "scrapingTime": <timeInS>,
 * [optional] "labels": {
 *          "<labelName>": {
 *              "value": "<value>"
 *          },
 *          "<labelName>": {
 *              "value": "<value>"
 *          },
 *          ...
 *      }
 * [optional] "currentLastTime": {
 *          "show": <true|false>
 *          [optional] "carryLabels": <true|false>
 *          [optional] "alias": "<aliasName>"
 *          [optional] "customDescription": "<customDescription>"
 *          [optional] "labels": {
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              ...
 *          }
 *      }
 * [optional] "readTime": {
 *          "show": <true|false>
 *          [optional] "carryLabels": <true|false>
 *          [optional] "alias": "<aliasName>"
 *          [optional] "customDescription": "<customDescription>"
 *          [optional] "labels": {
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              ...
 *          }
 *      }
 * [optional] "lastTrySuccessful": {
 *          "show": <true|false>
 *          [optional] "carryLabels": <true|false>
 *          [optional] "alias": "<aliasName>"
 *          [optional] "customDescription": "<customDescription>"
 *          [optional] "labels": {
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              ...
 *          }
 *      }
 * [optional] "readTimestamp": {
 *          "show": <true|false>
 *          [optional] "carryLabels": <true|false>
 *          [optional] "alias": "<aliasName>"
 *          [optional] "customDescription": "<customDescription>"
 *          [optional] "labels": {
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              "<labelName>": {
 *                  "value": "<value>"
 *              },
 *              ...
 *          }
 *      }
 * },
 * ...
 *
*/
struct ConfigNetId {
    uint8_t _1 = 0;
    uint8_t _2 = 0;
    uint8_t _3 = 0;
    uint8_t _4 = 0;
    uint8_t _5 = 0;
    uint8_t _6 = 0;

    explicit ConfigNetId(std::string str) {
        _1 = toNum(str);
        _2 = toNum(str);
        _3 = toNum(str);
        _4 = toNum(str);
        _5 = toNum(str);
        _6 = toNum(str);
    }
private:
    static uint8_t toNum (std::string& str) {
        size_t const delimitor =  str.find_first_of('.', 0);
        if (delimitor == std::string::npos) {
            return std::stoi(str);
        }
        const uint8_t result = std::stoi(str.substr(0, delimitor));
        str.erase(0, delimitor + 1);
        return result;
    }
};

/*!
 * @brief this class is for reading the configuration and applying it to the classes.
 */
class config_t {
public:
    void readConfig(std::string fileName);
    [[nodiscard]] inline ConfigNetId getLocalNetId() const {return localNetId;}
    [[nodiscard]] inline ConfigNetId getRemoteNetId() const {return remoteNetId;}
    [[nodiscard]] inline std::string getRemoteIp() const {return remoteIp;};
    [[nodiscard]] inline uint16_t getHttpPort() const {return httpPort;};
    [[nodiscard]] inline long getRefreshTimeResolution() const {return refreshTimeResolution;};

    void configureADSProvidor(AdsProvidor_t& AdsProvidor) const;
    void configurePrometheusEndpoint(PrometheusEndpoint_t& Endpoint) const;

private:
    void processConfig();
    void parseSymbol(const std::string& symbol);

    static prometheusMetricType toPrometheusMetricType(const std::string& data);
    static symbolDataType_t toSymbolDataType (const std::string& data);

    additionalDataMetric_t parseAdditionalDataMetric (const std::string& symbol, const std::string& additionalDataName);

    json configData;

    long refreshTimeResolution = 500;
    ConfigNetId localNetId {"0.0.0.0.0"};
    ConfigNetId remoteNetId {"0.0.0.0.0"};
    std::string remoteIp;
    uint16_t httpPort = 0;

    struct variable_t {
        std::string symbolADSName;
        std::string alias;
        prometheusMetricType metricType = prometheusMetricType::UNTYPED;
        std::string description;
        symbolDataType_t ADSType;
        std::chrono::high_resolution_clock::duration scrapingDuration;
        std::vector<label_t> labels;

        additionalDataMetric_t currentLastTime; // a metric for time between last and current read
        additionalDataMetric_t ReadTime; // time to read a symbol
        additionalDataMetric_t LastTryStatus; // status of last try
        additionalDataMetric_t LastReadTimestamp; // the timestamp of the last read
    };
    std::vector<variable_t> variables;

};


#endif //CPPADSPROMETEUSCONNECTOR_CONFIGURATION_H