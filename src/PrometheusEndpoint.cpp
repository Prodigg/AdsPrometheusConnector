//
// Created by prodigg on 23.02.26.
//
#include "PrometheusEndpoint.h"

std::string PrometheusEndpoint_t::getPrometheusMetricName(const prometheusMetric_t& metric) {
    if (metric.alias.empty()) {
        std::string metricName = metric.symbolName;
        std::ranges::replace(metricName, '.', '_');
        return metricName;
    }

    return std::string(metric.alias);
}

std::string PrometheusEndpoint_t::generateHelp(const prometheusMetric_t& metric) {
    if (metric.description.empty())
        return "";
    std::stringstream ss;
    ss << "# HELP " << getPrometheusMetricName(metric) << " " << escapeHelpStr(metric.description) << "\n";
    return ss.str();
}

std::string PrometheusEndpoint_t::generateType(const prometheusMetric_t& metric) {
    if (metric.metricType == prometheusMetricType::UNTYPED)
        return "";
    std::stringstream ss;
    ss << "# TYPE " << getPrometheusMetricName(metric) << " ";
    if (metric.metricType == prometheusMetricType::COUNTER)
        ss << "counter";
    else if (metric.metricType == prometheusMetricType::GAUGE)
        ss << "gauge";
    ss << "\n";
    return ss.str();
}

std::string PrometheusEndpoint_t::generateDataLine(const prometheusMetric_t& metric) const {
    std::stringstream ss;
    symbolData_t data;
    _dataBuffer.getSymbolData(metric.symbolName, data);
    ss << getPrometheusMetricName(metric) << generateLabel(metric.labels) << " " << data.symbolValue << " ";
    ss << std::chrono::duration_cast<std::chrono::microseconds>(data.lastReadTime.time_since_epoch()).count() << "\n";
    return ss.str();
}

std::string PrometheusEndpoint_t::generateAdditionalData(const prometheusMetric_t& metric, const additionalDataMetric_t& data, const std::string&& normalNamePrefix, const std::string&& normalDescription, const std::string& value) const {
    if (!data.show)
        return "";
    std::string additionalDataName;
    if (data.alias.empty())
        additionalDataName = getPrometheusMetricName(metric) + "_" + normalNamePrefix;
    else
        additionalDataName = data.alias;

    symbolData_t rawData;
    _dataBuffer.getSymbolData(metric.symbolName, rawData);

    std::stringstream ss;
    ss << "# HELP " << additionalDataName + " ";
    if (data.customDescription.empty())
        ss << normalDescription;
    else
        ss << escapeHelpStr(data.customDescription);
    ss << "\n";

    ss << "# TYPE " << additionalDataName << " gauge\n";

    ss << additionalDataName;
    if (data.carryLabels)
        ss << generateLabel(metric.labels);
    else
        ss << generateLabel(data.labels);
    ss << " " << value << std::chrono::duration_cast<std::chrono::microseconds>(rawData.lastReadTime.time_since_epoch()).count() << "\n";
    return ss.str();
}


std::string PrometheusEndpoint_t::generateEndpointData() const {
    std::stringstream ss;
    for (const prometheusMetric_t & metric: _metrics) {
        ss << generateHelp(metric);
        ss << generateType(metric);
        ss << generateDataLine(metric);
        // additional data
        symbolData_t data;
        _dataBuffer.getSymbolData(metric.symbolName, data);

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

bool PrometheusEndpoint_t::addSymbol(const prometheusMetric_t& metric) {
    if (metric.symbolName.empty())
        return false;
    _metrics.push_back(metric);
    return true;
}

PrometheusEndpoint_t::PrometheusEndpoint_t(ProcessDataBuffer_t& dataBuffer, const uint16_t port) :
    _dataBuffer(dataBuffer),
    addr(Pistache::Ipv4::any(), Pistache::Port(port)),
    endpoint(addr) {
    Pistache::Rest::Routes::Get(router, "/metrics", Pistache::Rest::Routes::bind(&PrometheusEndpoint_t::getData, this));

    endpoint.init(opts);
    endpoint.setHandler(router.handler());

    _thread.emplace(std::jthread(&PrometheusEndpoint_t::threadLoop, this));
}

void PrometheusEndpoint_t::getData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) {
    response.send(Pistache::Http::Code::Ok, generateEndpointData());
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

std::string PrometheusEndpoint_t::generateLabel(const std::vector<label_t>& labels) {
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

std::string PrometheusEndpoint_t::escapeHelpStr(const std::string_view str) {
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

std::string PrometheusEndpoint_t::escapeLabelStr(const std::string_view str) {
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