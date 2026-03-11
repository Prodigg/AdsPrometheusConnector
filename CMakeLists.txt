cmake_minimum_required(VERSION 3.16)
project(AdsPrometheusConnector)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(external/ADS/AdsLib)

add_executable(AdsPrometheusConnector
        src/main.cpp
        src/ProcessDataBuffer.h
        src/ProcessDataBuffer.cpp
        src/ADSProvidor.h
        src/ADSProvidor.cpp
        src/PrometheusEndpoint.h
        src/PrometheusEndpoint.cpp
        src/Configuration.h
        src/Configuration.cpp
        src/AdsVariableList.h
)

set(CMAKE_POLICY_VERSION 4.1)
set(CMAKE_POLICY_DEFAULT_CMP0020 NEW) # For FetchContent new behavior
set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0169 NEW)


include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

target_link_libraries(AdsPrometheusConnector PRIVATE
        PkgConfig::Pistache
        nlohmann_json::nlohmann_json
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ADSLIB REQUIRED IMPORTED_TARGET AdsLib)

target_link_libraries(AdsPrometheusConnector PRIVATE PkgConfig::ADSLIB)

