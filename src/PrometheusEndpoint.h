//
// Created by prodigg on 22.02.26.
//

#ifndef CPPADSPROMETEUSCONNECTOR_PROMETHEUSENDPOINT_H
#define CPPADSPROMETEUSCONNECTOR_PROMETHEUSENDPOINT_H

#include <thread>
#include <vector>

#include "ProcessDataBuffer.h"
#include "AdsVariableList.h"
#include "prometheusBuilder.h"
#include <pistache/endpoint.h>
#include <pistache/router.h>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

class grafanaMetricGenerator_t;

class PrometheusEndpoint_t {
public:
    explicit PrometheusEndpoint_t(ProcessDataBuffer_t& dataBuffer, uint16_t port);

    /*!
     * @brief adds a symbol to the endpoint
     * @param metric data for the metric to add
     * @return successful
     */
    bool addSymbol(const prometheusMetric_t& metric);

    /*!
     * @brief this method starts the endpoint
     */
    void serve() {
        endpoint.serve();
    }
private:

    /*!
     * @brief this is the main thread function
     */
    void threadLoop(std::stop_token stoken);

    /*!
     * @brief this is the callback to retrieve data
     */
    void getData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response) const;

    /*!
     * @brief get the prometheus metric name
     * @param metric
     * @return
     */
    static std::string getPrometheusMetricName(const prometheusMetric_t& metric);

    /*!
     * @brief generates the full endpoint data to serve
     * @return
     */
    std::string generateEndpointData() const;

    /*!
     * @brief generate prometheus symbol for additional data
     * @param data additional data
     * @return
     */
    std::string generateAdditionalData(const prometheusMetric_t& metric, const additionalDataMetric_t& data, const std::string&& normalNamePrefix, const std::string&& normalDescription, const std::string& value) const;


    /*!
     * @brief this is the callback for the root page, it provides a nice welcome text for the user
     */
    void getRootPageData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);

    /*!
     * @brief this is the callback for the additional information page, it provides additional data in a json formate
     */
    void getAdditionalInformationPageData(const Pistache::Rest::Request& request, Pistache::Http::ResponseWriter response);

    std::string generateAdditionalInformation();

    std::vector<prometheusMetric_t> _metrics;
    std::optional<std::jthread> _thread;
    ProcessDataBuffer_t& _dataBuffer;

    Pistache::Http::Endpoint::Options opts = Pistache::Http::Endpoint::options().threads(1);
    Pistache::Address addr;
    Pistache::Http::Endpoint endpoint;
    Pistache::Rest::Router router;
};


#endif //CPPADSPROMETEUSCONNECTOR_PROMETHEUSENDPOINT_H