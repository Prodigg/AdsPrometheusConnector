# AdsPrometheusConnector

## Overview
This program translates ADS variables into a Prometheus-compatible HTTP metrics endpoint. <br>
### What is ADS?
ADS is an automation protocol from Beckhoff and is used in PLCs from Beckhoff to communicate 
from the core TwinCAT function (TE1000) with extensions e.g. TF2000. 
More information can be found on the official Beckhoff [website](https://infosys.beckhoff.com/english.php?content=../content/1033/tcinfosys3/11291871243.html).
### Why is this needed?
It exposes ADS variables to Prometheus. This allows for straightforward IoT integrations.
Unlike OPC-UA, WebSocket, MQTT, database integrations, or custom HTTP servers, 
this connector does not require additional Beckhoff licenses.<br>
With this connector, it is possible to monitor ADS variables without any complex setup.

## Requirements
- Linux
- CMake (if building from source)
- [pistache](https://github.com/pistacheio/pistache) 

## Features

- ADS cyclic reading of variables
- providing an HTTP endpoint for prometheus to scrape
- prometheus labels supported

## Quick Start
To use this program use the prebuilt binary or compile it yourself with cmake. <br>
To start the program, launch it and provide in the first argument the path of the configuration file. 
```bash
./AdsPrometeusConnector config.json
```

### Compile with cmake

1. Run the ADS lib install script `./install_adslib.sh`
2. create a build directory with `mkdir build`
3. navigate into the build folder `cd build`
4. configure cmake with `cmake ..`
5. run the build with `cmake --build . -- -j 30`

### Running as a daemon
If you are using systemd, you can install this as a daemon. I have provided a example service configuration.
To start, copy the `AdsPrometheusConnector.service` into the `/etc/systemd/system/` folder. 
After that, copy the binary into the `/usr/local/bin/` folder. Make sure that the binary is a executable. <br><br>

After setting up the daemon, make a configuration file under `/etc/AdsPropetheusConnector/config.json`. 
You may also consider using the example config `textConfig.json` as a starting point.

## Configuration

```json
{
  "global": {
    "localNetId": "<NetID of this system>",
    "remoteNetId": "<NetID of SPS>",
    "remoteIp": "<IP of SPS>",
    "httpPort": 9090
  },

  "<name of ADS symbol>": {
    "alias": "[optional] <an alternative name for the name on the prometheus endpoint>",
    "type": "[optional] <untyped|counter|gauge>",
    "description": "[optional] <a description of the metric>",
    "ADSDatatype": "<the ADS type (see all supported types bellow)>",
    "scrapingTime": 1,
    "labels": {
      "<label name>": {
        "value": "<value of the label>"
      }
    }
  }
}
```

### Labels
Inside the `Labels` struct, a list of labels can be provided, the example above only shows one label.

### Scraping Time
The scraping time specifies how often the variable is read from ADS Server. 
The scraping time of the prometheus endpoint is specified in the config of prometheus.

### Supported ADS Datatypes
- BOOL
- UINT8
- UINT16
- UINT32
- UINT64
- FLOAT
- DOUBLE
- INT8
- INT16
- INT32
- INT64

### Example Config

#### Config
```json
{
  "global": {
    "localNetId": "192.168.0.10.1.1",
    "remoteNetId": "5.109.7.180.1.1",
    "remoteIp": "192.168.0.20",
    "httpPort": 9090
  },

  "MAIN.nMotorTemperature": {
    "alias": "motor_temperature",
    "type": "gauge",
    "description": "Motor temperature",
    "ADSDatatype": "DOUBLE",
    "scrapingTime": 1,
    "labels": {
      "machine": {
        "value": "A1"
      }
    }
  }
}
```
#### Output
```text
# HELP motor_temperature Motor temperature
# TYPE motor_temperature gauge
motor_temperature{machine="A1"} 42.3
```

## Configuring the PLC
For the ADS connector to work you need to add a static route to the Beckhoff PLC. 
Secure ADS is currently not supported.

### Adding a static route
A static route can be added in TcXaeShell. Here is an example config:
```text
  Name:           MyAdsClient # a name for the route
  AMS Net Id:     192.168.0.1.1.1 # NetId of your ADS client, here you can choose any valid NetID (for example use IP-Address.1.1 (the ip address of the system running the connector) e.g 192.168.0.10.1.1)
  Address:        192.168.0.1     # the IP-Address of the system running the connector
  Transport Type: TCP/IP
  Remote Route:   None / Server
  Unidirectional: false
  Secure ADS:     false
```

## Configuring prometheus
To use this endpoint edit the [config file](https://prometheus.io/docs/prometheus/latest/configuration/configuration/) of prometheus. 
Bellow is a minimal example:
```yaml
scrape_configs:
  - job_name: 'ads_connector'
    static_configs:
      - targets: ['localhost:9090']
```

## Limitations

This is a personal project built for a my use case and is not extensively tested.

- This connector is **not designed** for **safety-critical** or **mission-critical** systems.
- **Data loss may occur** (e.g., if the application crashes or the ADS connection fails).
- **No high availability** mechanisms are implemented.
- **Not suitable** for **high-frequency** or **real-time** data acquisition.
- **No security** mechanisms are implemented. HTTP and ADS communications are **not encrypted**.

It may be used in industrial environments where occasional **data loss is acceptable** and monitoring is non-critical.

### Alternatives

If this connector doesn't fit the use case consider using [TwinCAT Analytics](https://www.beckhoff.com/en-en/products/automation/twincat-analytics/) or
[TwinCAT IoT Data Agent](https://www.beckhoff.com/en-en/products/automation/twincat/tfxxxx-twincat-3-functions/tf6xxx-connectivity/tf6720.html).