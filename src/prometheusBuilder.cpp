//
// Created by prodigg on 08.04.26.
//
#include "prometheusBuilder.h"

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