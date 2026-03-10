cmake_minimum_required(VERSION 4.1)
project(CppAdsPrometheusConnector)

set(CMAKE_CXX_STANDARD 23)
#add_subdirectory(external/ADS/include/AdsLib)
include_directories(external/ADS/AdsLib)
#include_directories(external/libpistacheStatic/include)

add_executable(CppAdsPrometheusConnector
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


#FetchContent_Declare(
        #pistache
        #GIT_REPOSITORY https://github.com/oktal/pistache.git
        #        GIT_TAG master
#)
# Disable examples/tests
#set(Pistache_BUILD_TESTS OFF CACHE BOOL "" FORCE)
#set(Pistache_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(pistache)

find_package(PkgConfig)
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

target_link_libraries(CppAdsPrometheusConnector PRIVATE
        PkgConfig::Pistache
        nlohmann_json::nlohmann_json
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ADSLIB REQUIRED IMPORTED_TARGET AdsLib)

#target_include_directories(CppAdsPrometheusConnector PRIVATE PkgConfig::ADSLIB)
target_link_libraries(CppAdsPrometheusConnector PRIVATE PkgConfig::ADSLIB)

#target_link_libraries(CppAdsPrometheusConnector PRIVATE nlohmann_json::nlohmann_json)
#target_link_libraries(CppAdsPrometheusConnector PRIVATE ${CMAKE_SOURCE_DIR}/external/ADS/lib/libAdsLib.a)
#target_link_libraries(CppAdsPrometheusConnector PRIVATE ${CMAKE_SOURCE_DIR}/external/libpistacheStatic/libpistache.a)
