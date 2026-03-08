#include <iostream>

#include "AdsLib.h"
#include "AdsDevice.h"
#include "AdsVariable.h"

//#include <pistache/endpoint.h>
//#include <pistache/router.h>

#include "ADSProvidor.h"
#include "ProcessDataBuffer.h"
#include "PrometheusEndpoint.h"
#include "Configuration.h"

using namespace Pistache;


void test(const Rest::Request& request, Http::ResponseWriter response) {
    response.send(Http::Code::Ok, "endpoint is ok\n");
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.size() != 2) {
        std::cerr << "Usage: " << argv[0] << " [pathToConfig]" << std::endl;
        //exit(EXIT_FAILURE);
        args.emplace_back("/home/prodigg/CLionProjects/CppAdsPrometeusConnector/testConfig.json");
    }

    config_t config;
    config.readConfig(args.at(1));

    ProcessDataBuffer_t processDataBuffer;

    AdsProvidor_t adsProvidor(processDataBuffer,
        {config.getRemoteNetId()._1, config.getRemoteNetId()._2, config.getRemoteNetId()._3, config.getRemoteNetId()._4, config.getRemoteNetId()._5, config.getRemoteNetId()._6},
        config.getRemoteIp(),
        {config.getLocalNetId()._1, config.getLocalNetId()._2, config.getLocalNetId()._3, config.getLocalNetId()._4, config.getLocalNetId()._5,config.getLocalNetId()._6});

    config.configureADSProvidor(adsProvidor);

    PrometheusEndpoint_t endpoint(processDataBuffer, config.getHttpPort());
    config.configurePrometheusEndpoint(endpoint);

    //endpoint.serve();

    while (true)
        ;

    return 0;
}
