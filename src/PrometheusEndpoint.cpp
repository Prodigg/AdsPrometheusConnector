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
    std::string value;
    _dataBuffer.getSymbolValue(metric.symbolName, value);
    ss << getPrometheusMetricName(metric) << generateLabel(metric) << " " << value << "\n";
    return ss.str();
}

std::string PrometheusEndpoint_t::generateEndpointData() const {
    std::stringstream ss;
    for (const prometheusMetric_t & metric: _metrics) {
        ss << generateHelp(metric);
        ss << generateType(metric);
        ss << generateDataLine(metric);
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
        }
    }
}

std::string PrometheusEndpoint_t::generateLabel(const prometheusMetric_t& metric) {
    if (metric.labels.empty())
        return "";
    std::string labelStr = "{";
    for (const auto &[label, value]: metric.labels) {
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