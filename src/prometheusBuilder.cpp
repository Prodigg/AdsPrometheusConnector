//
// Created by prodigg on 08.04.26.
//
#include "prometheusBuilder.h"

void grafanaMetricGenerator_t::generateMetric(const std::string_view metricName, const std::string_view data, const std::string_view helpStr, const prometheusMetricType type, const std::vector<label_t>& labels, const std::chrono::system_clock::time_point readTime, std::string& outStr) {
    generateHelp(helpStr, metricName, outStr);
    generateType(type, metricName, outStr);
    generateDataLine(metricName, data, labels, readTime, outStr);
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

void grafanaMetricGenerator_t::generateHelp(const std::string_view helpStr, const std::string_view metricName, std::string& outStr)  {
    if (helpStr.empty())
        return;
    outStr.reserve(metricName.length() + helpStr.length() + 15);
    outStr += "# HELP ";
    outStr += metricName;
    outStr += " ";
    outStr += escapeHelpStr(helpStr);
    outStr += "\n";
}

void grafanaMetricGenerator_t::generateType(const prometheusMetricType type, const std::string_view metricName, std::string& outStr) {
    outStr.reserve(metricName.length() + 20);
    outStr += "# TYPE ";
    outStr += metricName;
    outStr += "\n";
    if (type == prometheusMetricType::COUNTER)
        outStr += "counter";
    else if (type == prometheusMetricType::GAUGE)
        outStr += "gauge";
    else if (type == prometheusMetricType::UNTYPED)
        outStr += "untyped";
    outStr += "\n";
}

void grafanaMetricGenerator_t::generateDataLine(const std::string_view metricName, const std::string_view data, const std::vector<label_t>& labels, const std::chrono::system_clock::time_point dataReadTime, std::string& outStr) {
    size_t sizeOfLabels = 0;
    for (const auto &[label, value]: labels) {
        sizeOfLabels += label.length();
        sizeOfLabels += value.length();
        sizeOfLabels += 7;
    }

    std::string str;
    outStr.reserve(sizeOfLabels + metricName.length() + data.length() + 20);
    outStr += metricName;

    generateLabel(labels, outStr);

    outStr += " ";
    outStr += data;
    outStr += " ";
    outStr += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(dataReadTime.time_since_epoch()).count());
    outStr += "\n";
}

void grafanaMetricGenerator_t::generateLabel(const std::vector<label_t>& labels, std::string& outStr)  {
    if (labels.empty())
        return;

    outStr += "{";
    for (const auto &[label, value]: labels) {
        outStr += label;
        outStr += "=\"";
        outStr += escapeLabelStr(value);
        outStr += "\",";
    }

    if (outStr.back() == ',')
        outStr.pop_back();
    outStr += "}";
}