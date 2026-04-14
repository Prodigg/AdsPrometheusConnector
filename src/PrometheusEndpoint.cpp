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
    const auto startTime = std::chrono::steady_clock::now();
    std::string str;
    generateEndpointData(str);
    response.send(Pistache::Http::Code::Ok, str);

    auto timeTaken = std::chrono::steady_clock::now() - startTime;
}


std::string PrometheusEndpoint_t::getPrometheusMetricName(const prometheusMetric_t& metric) {
    if (metric.alias.empty()) {
        std::string metricName = metric.symbolName;
        std::ranges::replace(metricName, '.', '_');
        return metricName;
    }

    return std::string(metric.alias);
}

void PrometheusEndpoint_t::generateAdditionalData(const prometheusMetric_t& metric, const additionalDataMetric_t& data, const std::string&& normalNamePrefix, const std::string&& normalDescription, const std::string& value, std::string& outStr) const {
    if (!data.show)
        return;

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
        symbolData.lastReadTime,
        outStr);
}


void PrometheusEndpoint_t::generateEndpointData(std::string& outStr) const {

    // group metrics
    for (const auto &[readTime, dataReadTime, worker, readGroup, readGroupSymbols, readGroupScrapingTime, wasLastReadSuccessful]: _dataBuffer.dumpReadGroupMetrics()) {
        grafanaMetricGenerator_t::generateMetric(
            "connector_read_group_read_time_nanoseconds",
            std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(readTime).count()),
            "the readtime of a read group",
            prometheusMetricType::GAUGE,
            {{"worker", worker}, {"group", readGroup}},
            dataReadTime,
            outStr);
        grafanaMetricGenerator_t::generateMetric(
            "connector_read_group_read_successful",
            std::to_string(wasLastReadSuccessful),
            "1 if all variables of the read group where read successful",
            prometheusMetricType::GAUGE,
            {{"worker", worker}, {"group", readGroup}},
            dataReadTime,
            outStr);
    }

    for (const prometheusMetric_t & metric: _metrics) {
        // data
        symbolData_t data;
        _dataBuffer.getSymbolData(metric.symbolName, data);

        grafanaMetricGenerator_t::generateMetric(
            getPrometheusMetricName(metric),
            data.symbolValue,
            metric.description,
            metric.metricType,
            metric.labels,
            data.lastReadTime,
            outStr);

        generateAdditionalData(metric, metric.currentLastTime,
            "current_last_time",
            "The time between this and last read Type in MS",
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(data.lastCurrentTime).count()),
            outStr);
        generateAdditionalData(metric, metric.ReadTime,
            "read_time",
            "The time it took to read the symbol Type in NS",
            std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(data.symbolReadTime).count()),
            outStr);
        generateAdditionalData(metric, metric.LastTryStatus,
            "last_try_status",
            "true if the last try was successful",
            std::to_string(data.wasLastReadSuccessful),
            outStr);
        generateAdditionalData(metric, metric.LastReadTimestamp,
            "timestamp",
            "UNIX timestamp of last read",
            std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(data.lastReadTime.time_since_epoch()).count()),
            outStr);
    }
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
     *      "readGroups": [
     *          {
     *              "worker": "<worker>",
     *              "readGroup": <number of readGroup>,
     *              "scrapingTime_ms": <scrapingTime in ms>,
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


    for (const auto &[readTime, dataReadTime, worker, readGroup, readGroupSymbols, readGroupScrapingTime, wasLastReadSuccessful]: _dataBuffer.dumpReadGroupMetrics()) {
        additionalInformation.at("readGroups").emplace_back(json({
            {"worker", worker},
            {"readGroup", readGroup},
            {"scrapingTime_ms", std::chrono::duration_cast<std::chrono::milliseconds>(readGroupScrapingTime).count()},
            {"symbols", readGroupSymbols},
            {"wasLastReadSuccessful", wasLastReadSuccessful}
        })
        );
    }

    return additionalInformation.dump(4);
}
