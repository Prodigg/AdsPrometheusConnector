//
// Created by prodigg on 23.02.26.
//
#include "PrometheusEndpoint.h"

PrometheusEndpoint_t::PrometheusEndpoint_t(ProcessDataBuffer_t& dataBuffer, const uint16_t port) :
    _dataBuffer(dataBuffer),
    addr(Pistache::Ipv4::any(), Pistache::Port(port)),
    endpoint(addr) {
    Pistache::Rest::Routes::Get(router, "/metrics", Pistache::Rest::Routes::bind(&PrometheusEndpoint_t::getData, this));
    Pistache::Rest::Routes::Get(router, "/", Pistache::Rest::Routes::bind(&PrometheusEndpoint_t::getRootPageData, this));
    Pistache::Rest::Routes::Get(router, "/additionalInformation", Pistache::Rest::Routes::bind(&PrometheusEndpoint_t::getAdditionalInformationPageData, this));

    endpoint.init(opts);
    endpoint.setHandler(router.handler());

    _thread.emplace(std::jthread(&PrometheusEndpoint_t::threadLoop, this));
    std::cout << "INFO: Started Prometheus endpoint at port: " << port << "\n";
}

bool PrometheusEndpoint_t::addSymbol(const prometheusMetric_t& metric) {
    if (metric.symbolName.empty())
        return false;
    _metrics.push_back(metric);
    return true;
}

void PrometheusEndpoint_t::threadLoop(std::stop_token stoken) {
    while (true) {
        try {
            endpoint.serve();
        } catch (std::exception &e) {
            std::cerr << "Server crashed: " << e.what() << std::endl;
            return;
        }
    }
}

void PrometheusEndpoint_t::getData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) const {
    response.send(Pistache::Http::Code::Ok, generateEndpointData());
}


std::string PrometheusEndpoint_t::getPrometheusMetricName(const prometheusMetric_t& metric) {
    if (metric.alias.empty()) {
        std::string metricName = metric.symbolName;
        std::ranges::replace(metricName, '.', '_');
        return metricName;
    }

    return std::string(metric.alias);
}

std::string PrometheusEndpoint_t::generateAdditionalData(const prometheusMetric_t& metric, const additionalDataMetric_t& data, const std::string&& normalNamePrefix, const std::string&& normalDescription, const std::string& value) const {
    if (!data.show)
        return "";

    std::string additionalDataName;
    if (data.alias.empty())
        additionalDataName = getPrometheusMetricName(metric) + "_" + normalNamePrefix;
    else
        additionalDataName = data.alias;

    symbolData_t symbolData;
    _dataBuffer.getSymbolData(metric.symbolName, symbolData);

    const std::string_view helpStr = data.customDescription.empty() ? normalDescription : data.customDescription;
    const std::vector<label_t>& labels =  data.carryLabels ? metric.labels : data.labels;

    return grafanaMetricGenerator_t::generateMetric(
        additionalDataName,
        value,
        helpStr,
        prometheusMetricType::GAUGE,
        labels,
        symbolData.lastReadTime);
}


std::string PrometheusEndpoint_t::generateEndpointData() const {
    std::stringstream ss;

    // group metrics
    for (const auto &[readTime, dataReadTime, worker, readGroup, readGroupSymbols, readGroupScrapingTime]: _dataBuffer.dumpReadGroupMetrics()) {
        ss << grafanaMetricGenerator_t::generateMetric(
            "connector_read_group_read_time_nanoseconds",
            std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(readTime).count()),
            "the readtime of a read group",
            prometheusMetricType::GAUGE,
            {{"worker", worker}, {"group", readGroup}},
            dataReadTime);
    }

    for (const prometheusMetric_t & metric: _metrics) {
        // data
        symbolData_t data;
        _dataBuffer.getSymbolData(metric.symbolName, data);

        ss << grafanaMetricGenerator_t::generateMetric(
            getPrometheusMetricName(metric),
            data.symbolValue,
            metric.description,
            metric.metricType,
            metric.labels,
            data.lastReadTime);

        ss << generateAdditionalData(metric, metric.currentLastTime,
            "current_last_time",
            "The time between this and last read Type in MS",
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(data.lastCurrentTime).count()));
        ss << generateAdditionalData(metric, metric.ReadTime,
            "read_time",
            "The time it took to read the symbol Type in NS",
            std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(data.symbolReadTime).count()));
        ss << generateAdditionalData(metric, metric.LastTryStatus,
            "last_try_status",
            "true if the last try was successful",
            std::to_string(data.wasLastReadSuccessful));
        ss << generateAdditionalData(metric, metric.LastReadTimestamp,
            "timestamp",
            "UNIX timestamp of last read",
            std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(data.lastReadTime.time_since_epoch()).count()));
    }
    return ss.str();
}

void PrometheusEndpoint_t::getRootPageData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    response.send(Pistache::Http::Code::Ok,
        R"(Welcome to the ADS Prometheus connector.
Go to /metrics for the prometheus http endpoint.
For additional information go to /additionalInformation. The additional information page provides data in the json formate.
)");

}
void PrometheusEndpoint_t::getAdditionalInformationPageData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    response.send(Pistache::Http::Code::Ok, generateAdditionalInformation());
}

std::string PrometheusEndpoint_t::generateAdditionalInformation() {
    /*
     * Formate:
     * {
     *      readGroups: [
     *          {
     *              "worker": "<worker>",
     *              "readGroup": <number of readGroup>,
     *              "scrapingTime_s": <scrapingTime in s>,
     *              "symbols": [
     *                  "symbol1",
     *                  "symbol2",
     *                  ...
     *              ]
     *          },
     *          ...
     *      ]
     * }
     */
    json additionalInformation;
    additionalInformation.emplace("readGroups", json::array({}));


    for (const auto &[readTime, dataReadTime, worker, readGroup, readGroupSymbols, readGroupScrapingTime]: _dataBuffer.dumpReadGroupMetrics()) {
        additionalInformation.at("readGroups").emplace_back(json({
            {"worker", worker},
            {"readGroup", readGroup},
            {"scrapingTime_s", std::chrono::duration_cast<std::chrono::seconds>(readGroupScrapingTime).count()},
            {"symbols", readGroupSymbols}
        })
        );
    }

    return additionalInformation.dump(4);
}

std::string grafanaMetricGenerator_t::generateMetric(const std::string_view metricName, const std::string_view data, const std::string_view helpStr, const prometheusMetricType type, const std::vector<label_t>& labels, const std::chrono::system_clock::time_point readTime) {
    std::stringstream ss;
    ss << generateHelp(helpStr, metricName);
    ss << generateType(type, metricName);
    ss << generateDataLine(metricName, data, labels, readTime);
    return ss.str();
}


std::string grafanaMetricGenerator_t::escapeLabelStr(std::string_view str)  {
    if (str.empty())
        return "";
    std::string workStr(str);
    for (int i = 0; i < workStr.length(); i++) {
        if (workStr[i] == '"' || workStr[i] == '\\') {
            // escaping needed
            workStr.insert(i, "\\", 1);
            i += 2;
        }
    }
    return workStr;
}

std::string grafanaMetricGenerator_t::escapeHelpStr(std::string_view str)  {
    if (str.empty())
        return "";
    std::string workStr(str);
    for (int i = 0; i < workStr.length(); i++) {
        if (workStr[i] == '\\') {
            // escaping needed
            workStr.insert(i, "\\", 1);
            i += 2;
        }
    }
    return workStr;
}

std::string grafanaMetricGenerator_t::generateHelp(const std::string_view helpStr, const std::string_view metricName)  {
    if (helpStr.empty())
        return "";
    std::stringstream ss;
    ss << "# HELP " << metricName << " " << escapeHelpStr(helpStr) << "\n";
    return ss.str();
}

std::string grafanaMetricGenerator_t::generateType(const prometheusMetricType type, const std::string_view metricName) {
    if (type == prometheusMetricType::UNTYPED)
        return "";
    std::stringstream ss;
    ss << "# TYPE " << metricName << " ";
    if (type == prometheusMetricType::COUNTER)
        ss << "counter";
    else if (type == prometheusMetricType::GAUGE)
        ss << "gauge";
    ss << "\n";
    return ss.str();
}

std::string grafanaMetricGenerator_t::generateDataLine(const std::string_view metricName, const std::string_view data, const std::vector<label_t>& labels, std::chrono::system_clock::time_point dataReadTime) {
    std::stringstream ss;
    ss << metricName << generateLabel(labels) << " " << data << " ";
    ss << std::chrono::duration_cast<std::chrono::milliseconds>(dataReadTime.time_since_epoch()).count() << "\n";
    return ss.str();
}

std::string grafanaMetricGenerator_t::generateLabel(const std::vector<label_t>& labels)  {
    if (labels.empty())
        return "";
    std::string labelStr = "{";
    for (const auto &[label, value]: labels) {
        labelStr += label;
        labelStr += "=\"";
        labelStr += escapeLabelStr(value);
        labelStr += "\",";
    }

    if (labelStr.at(labelStr.length() - 1) == ',')
        labelStr = labelStr.substr(0, labelStr.length() - 1);
    labelStr += "}";
    return labelStr;
}