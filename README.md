Inital 'draft' for an IOT device foundation.
This is a work in progress and more changes are planned.
This sketch relies on the following:
LittleFS
ESPAsyncWebServer
AsyncTCP
RTCMemory
ArduinoJson

This project aims to act as a generic platform to build an IOT type device that doesn't require hard coding data into the sketch.
I have been unable to locate an example IoT device project that is fully runtime configurable.
I also wish to be able to support the ESP-01s for small footprint applications and increase the WAF (wife approval factor).
that limitation means facilitating mode selection without an added physical switch or button.

The overall goal is for a user to be able to take this project, update an array of tuples or some sort of POD object to add desired config items, pull items from the json config data, then write their device code without working about the config stuff.

- In the absence of config data, the device will come up in AP mode with a network name that indicated it is in config mode.
- The device will serve a web page that presents a set of fields for the user to input their config data, save, and reboot the device.
- The device can can be placed back into a version of config mode using the reset button.
  - A double reset uses programmed WiFi credentials but runs the web server to allow for reconfiguration
    - Intended to reconfigure parameters not related to WiFi like device name or MQTT parameters
  - A triple reset returns the device to AP mode with web server but the config is preserved
    - Intended for updating WiFi settings without fully erasing the config
  - A quad reset erases the stored JSON file that holds the config data

This project aims to facilitate easily adding items to the configuration for the specific application.
Config data is stored as JSON key-value pairs.
An example if using MQTT:
- MQTT server
- MQTT authentication info
- MQTT topics
The system should be flexible enough to accommodate user needs as long as the data is a reasonable length.

In 'device mode', the JSON data is passed directly to the device mode code as it's application specific.


Currently non-functional as rewrite is in progress.

Short term Roadmap: 
  - Create config object class
    - JSON object key name
    - callback to add table row to config HTML
    - callback to handle form data item
    - designed to be automatically created using a user defined list of parameters
  - Refactor HTML subsystem
    - Connect it better with the config objects
    - revisit if this should remain a set of functions or is better as an object
    - Determine if a switch to HTTPS is possible as POST method still sends data in the clear.
  - Determine best way to organize control flow based on mode selection
    - Should main ino file dispatch to mode specific code in dedicated .cpp files? 
    - Consider if there is an optimal way to make device mode code work

Long term roasdmap:
  - Test impact with battery based apps
    - Impact from having to read a config file and select a boot mode is unknown.
  - Work on 'network down' mode.
    - This is intended as a battery saving feature
      - Introduces progressively longer sleep periods if the programmed network is down
      - Prior attempts would just fail after long periods of disconnection.
  - Enable OTA updates
    - may not be possible on some devices due to space and power issues. 
  - Investigate ways to backup and restore the config
  
  
Examples of my planned applications:
Clock using NTP to keep time
Per room temperature monitoring
Home particulate air quality monitoring
home CO2 monitoring
remote control of window fan (spoofing IR protocol)
PC thermal monitoring (secondary set of sensors)