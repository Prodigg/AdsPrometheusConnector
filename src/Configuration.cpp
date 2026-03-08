//
// Created by prodigg on 24.02.26.
//

#include "Configuration.h"

#include <fstream>

/*
 * Config formate:
 * "global": {
 *      "localNetId": "<localNetID>",
 *      "remoteNetId": "<remoteNetID>",
 *      "remoteIPv4": "<remoteIPv4>",
 *      "httpPort": "<httpPort>"
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

void config_t::readConfig(std::string fileName) {
    if (fileName.empty()) {
        std::cerr<<"ERROR: Configuration file name is empty"<<std::endl;
        exit(EXIT_FAILURE);
    }

    std::ifstream file(fileName);

    if (!file.is_open()) {
        std::cerr<<"ERROR: Could not open configuration file "<<fileName<<std::endl;
        exit(EXIT_FAILURE);
    }

    try {
        configData = json::parse(file);
    } catch (const json::exception& e) {
        std::cerr<<"ERROR: failed to parse "<< fileName <<"\n";
        std::cerr<<e.what()<<std::endl;
    }


    file.close();
    processConfig();
}

void config_t::processConfig() {
    // check global struct
    if (!configData.contains("global")) {
        std::cerr<<"ERROR: Global struct is not present in config but is mandatory."<<std::endl;
        exit(EXIT_FAILURE);
    }
    // parse global struct
    try {
        localNetId = ConfigNetId(configData.at("global").at("localNetId"));
        remoteNetId = ConfigNetId(configData.at("global").at("remoteNetId"));
        remoteIp = configData.at("global").at("remoteIp");
        httpPort = configData.at("global").at("httpPort");
    } catch (const json::exception& e) {
        std::cerr<<"ERROR: failed to parse global structure\n";
        std::cerr<<e.what()<<std::endl;
        exit(EXIT_FAILURE);
    }

    //parse the ads symbols
    for (const auto & value: configData.items()) {
        if (value.key() == "global") continue;
        parseSymbol(value.key());
    }
}

void config_t::parseSymbol(const std::string& symbol) {
    variable_t variable;
    // parse symbol
    variable.symbolADSName = symbol;
    try {
        if (configData.at(symbol).contains("alias"))
            variable.alias = configData.at(symbol).at("alias");
        if (configData.at(symbol).contains("type"))
            variable.metricType = toPrometheusMetricType(configData.at(symbol).at("type"));
        if (configData.at(symbol).contains("description"))
            variable.description = configData.at(symbol).at("description");
        variable.ADSType = toSymbolDataType(configData.at(symbol).at("ADSDatatype"));
        variable.scrapingDuration = std::chrono::seconds(configData.at(symbol).at("scrapingTime"));
    } catch (const json::exception& e) {
        std::cerr<<"ERROR: failed to parse" << symbol << " structure\n";
        std::cerr<<e.what()<<std::endl;
        exit(EXIT_FAILURE);
    }

    // check labels
    if (configData.at(symbol).contains("labels")) {
        for (const auto& value: configData.at(symbol).at("labels").items()) {
            if (value.key() == "alias") continue;
            if (value.key() == "type") continue;
            if (value.key() == "description") continue;
            if (value.key() == "ADSDatatype") continue;
            if (value.key() == "scrapingTime") continue;

            if (!value.value().contains("value")) {
                std::cerr<<"ERROR: missing value in " << value.key() << " at symbol: "<< symbol << std::endl;
                exit(EXIT_FAILURE);
            }
            variable.labels.push_back({value.key(), value.value().at("value")});
        }
    }
    // parse additional data
    variable.currentLastTime = parseAdditionalDataMetric(symbol, "currentLastTime");
    variable.ReadTime = parseAdditionalDataMetric(symbol, "readTime");
    variable.LastTryStatus = parseAdditionalDataMetric(symbol, "lastTrySuccessful");
    variable.LastReadTimestamp = parseAdditionalDataMetric(symbol, "readTimestamp");

    variables.push_back(variable);
}

additionalDataMetric_t config_t::parseAdditionalDataMetric (const std::string& symbol, const std::string& additionalDataName) {
    additionalDataMetric_t additionalData;
    if (!configData.at(symbol).contains(additionalDataName)) {
        additionalData.show = false;
        return additionalData;
    }
    if (!configData.at(symbol).at(additionalDataName).contains("show")) {
        std::cerr<<"ERROR: missing show value in " << additionalDataName << " at symbol: "<< symbol << std::endl;
        exit(EXIT_FAILURE);
    }

    additionalData.show = configData.at(symbol).at(additionalDataName).at("show");

    if (!configData.at(symbol).at(additionalDataName).contains("carryLabels"))
        additionalData.carryLabels = true;
    else
        additionalData.carryLabels = configData.at(symbol).at(additionalDataName).at("carryLabels");

    if (!configData.at(symbol).at(additionalDataName).contains("alias"))
        additionalData.alias = "";
    else
        additionalData.alias = configData.at(symbol).at(additionalDataName).at("alias");

    if (!configData.at(symbol).at(additionalDataName).contains("customDescription"))
        additionalData.customDescription = "";
    else
        additionalData.customDescription = configData.at(symbol).at(additionalDataName).at("customDescription");

    if (configData.at(symbol).at(additionalDataName).contains("labels")) {
        for (const auto& value: configData.at(symbol).at(additionalDataName).at("labels").items()) {
            if (value.key() == "alias") continue;
            if (value.key() == "type") continue;
            if (value.key() == "description") continue;
            if (value.key() == "ADSDatatype") continue;
            if (value.key() == "scrapingTime") continue;

            if (!value.value().contains("value")) {
                std::cerr<<"ERROR: missing value in " << value.key() << " at symbol: "<< symbol << "." << additionalDataName << std::endl;
                exit(EXIT_FAILURE);
            }
            additionalData.labels.push_back({value.key(), value.value().at("value")});
        }
    }
    return additionalData;
}

void config_t::configureADSProvidor(AdsProvidor_t& AdsProvidor) const {
    for (const variable_t & variable: variables) {
        AdsProvidor.addSymbol(variable.symbolADSName, variable.ADSType, variable.scrapingDuration);
    }
}

void config_t::configurePrometheusEndpoint(PrometheusEndpoint_t& Endpoint) const {
    for (const variable_t & variable: variables) {
        Endpoint.addSymbol({variable.metricType, variable.symbolADSName, variable.description, variable.alias, variable.labels, variable.currentLastTime, variable.ReadTime, variable.LastTryStatus, variable.LastReadTimestamp});
    }
}

prometheusMetricType config_t::toPrometheusMetricType(const std::string& data) {
    if (data == "untyped")
        return prometheusMetricType::UNTYPED;
    else if (data == "counter")
        return prometheusMetricType::COUNTER;
    else if (data == "gauge")
        return prometheusMetricType::GAUGE;

    std::cerr << "ERROR: unrecognized metric type: " << data << std::endl;
    exit(EXIT_FAILURE);
}

symbolDataType_t config_t::toSymbolDataType (const std::string& data) {

    if (data == "BOOL") return symbolDataType_t::e_bool;
    else if (data == "UINT8") return symbolDataType_t::e_uint8_t;
    else if (data == "UINT16") return symbolDataType_t::e_uint16_t;
    else if (data == "UINT32") return symbolDataType_t::e_uint32_t;
    else if (data == "UINT64") return symbolDataType_t::e_uint64_t;
    else if (data == "FLOAT") return symbolDataType_t::e_float;
    else if (data == "DOUBLE") return symbolDataType_t::e_double;
    else if (data == "INT8") return symbolDataType_t::e_int8_t;
    else if (data == "INT16") return symbolDataType_t::e_int16_t;
    else if (data == "INT32") return symbolDataType_t::e_int32_t;
    else if (data == "INT64") return symbolDataType_t::e_int64_t;

    std::cerr << "ERROR: unrecognized ADS type: " << data << std::endl;
    exit(EXIT_FAILURE);
}