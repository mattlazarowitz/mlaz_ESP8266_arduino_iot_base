# Introduction (skip if you want functional and technical details)
The original problem was monitoring the temperature in an infant’s bedroom to make sure they were safe when sleeping. It started with an ESP-01s and a DHT-11 based board that it plugged into. I had no idea how…suboptimal my choices were at the time. After soldering and experiments in deep sleep followed by some more electronics surgery to connect an I2C sensor instead (the DHT-11 boards had a pretty good LDO regulator on them) and I had a reasonable hardware platform. And with updates to the Arduino framework combined with some wifi tricks, I had built a pretty good device in terms of power usage.

I’ve kept MQTT because it ‘just works’ (most of the time anyway). From there, I can get data to different places as needed. I’ve been tinkering with Node-Red but other platforms like Home Assistant should work too.

But after migrating my server from a Pi to a PC and centralizing things, I ended up having to reprogram a bunch of devices because I had followed the examples that hard coded everything. 

And that is where this project comes in. 
I wanted to be able to have a single project for a particular sensor config that I can program then config with things like WiFi credentials, MQTT server address, MQTT topics, and other things on the first boot.

I wanted to be able to change things like MQTT parameters on the device if I did something like migrate an MQTT server. 

I wanted to be able to update a WiFi address without updating things like MQTT settings if needed for tasks like moving things to different IOT specific networks at home. 

And finally, I figured I’d need a way to perform a ‘factory reset’. 

I wanted to be able to roll a new device based on a new sensor or set of sensors quickly, and I wanted to build devices that were low power as well as ones that could be ‘always on’.

I’ve been able to achieve that reasonably well here even if I have something that isn’t very Arduino-like anymore. For now, I think that tradeoff is OK.

There are some additional quirks that are the result of my desire to continue supporting an ESP-01s based sensor as well.

But I wanted to approach this with the mindset of creating a product.

# Device overview
The device uses LittleFS to store the configuration data and it is used to store the HTML page in an area where it will not impact program RAM. 
Configuration is stored as JSON data.
It uses RTC RAM for wifi state data and the ‘backoff’ mechanism.
It uses the AsyncWebServer library to provide a UI for the user to configure the device.

Not mentioned is the system for sending data.
While I use MQTT, there is no reason this should be tied to that. I’ll make references to it in the documentation to illustrate how I use this framework for a device, but you are free to implement whatever you would like. 

The device will configure the radio in AP mode when unconfigured or when the user has selected a config mode that will not connect to WiFi. 
The device will configure the radio for station mode when in device mode, or a reconfigure mode that preserves wifi settings.

After initial programming the device will not have a config file and will come up in config mode with the radio running in AP mode. Details about the wifi network, MQTT server, and parameters customized by the firmware are set here. Once the config is saved and the device rebooted, it will come up in device mode. 

By resetting the device twice in quick succession, it enters a mode where it will use the stored wifi credentials to connect to the network, but is in config mode. The device can be accessed from the wifi network it is connected to and be reconfigured as needed. This covers scenarios where MQTT settings or other non-network parameters need to be updated without having to go through the process of connecting to the device as an AP.

By resetting the device three times in quick succession, the device will present as an AP, enter config mode, but will retain all settings. This is useful for scenarios like updating the device when the prior wifi network it connected to is no longer available but you may want to retain other configuration data.

By resetting the device four times in quick succession, the device will erase its config file and start fresh.

In device mode operation using the low power system, there is an additional ‘backoff’ scheme that detects infrastructure issues and will extend sleep cycles for longer and longer while the outage goes on. This is to try and save battery power. It uses a retry count saved to RTC RAM to determine how long it has been retrying and the current delay it enforces. Restoring normal behavior in the event of an issue is to simply power cycle the device.

# Required libraries this framework:
- Http-Requests – https://github.com/dowerner/Arduino-Http-Requests
- ESPAsyncWebServer – lacamera
- RTCMemory – Fabiano Riccardi
- Arduinojson – Benoit Blanchon

# Architecture
The HTML server uses the HtmlRequests library and leverages its template capabilities to replace parts of the HTML at runtime. It actually uses 2 tiers of template items with the first being built into the HTML data and the second being constructed in buildInputFormEntries(). This allows for the fields in the HTML page to get built dynamically from the configItems array so the HTML page doesn’t need to be rewritten when using this to build a new device.

The configItems array is also used to determine the contents of the config.json file that stores settings. 

The configuration data is written to flash using LittleFS. Additionally, LittleFS is used to store the base HTML data. The LittleFS partition must be set up and data installed to the device after flashing the device. 

The files HtmlRequests.cpp and HtmlRequests.hpp contain the code to service the HTML requests and the callbacks needed to fill out the templates. 

jsonFileFuncs encapsulates the json config file operations while rtcInterface encapsulates the code to interface with the RTC RAM for state saving and loading.

The files backoff.cpp and backoff.hpp control how quickly the longer sleep durations scale and the duration of the extra delay at base level. 

deviceMode.cpp is where you will write your Arduino code. The setupDevMode() function is the same as the standard Arduino's Setup() function and loopDevMode() is the same as the Arduino’s Loop () call.

The configItems.hpp does a lot of the heavy lifting for the config files and HTML data. The intent is that items get added to the configItems vector and the code generates both the appropriate HTML and JSON needed by the device which simplifies the porting required.

The ino sketch file is mostly plumbing to select the right boot mode and get the code flowing to the right place. The one thing worth mentioning is the timer ISR. This exists to facilitate using resets to select modes. The firmware will increment a variable named unhandledResetCount which is initialized with data from the RTC RAM. The time it takes to get through Setup() is highly variable which makes the timing of a reset press extremely hard to get right. Not all firmware architectures will make use of the loop() function so checking there will cause problems as well. So a one shot timer set for a 750ms duration is used to create a window in which the user can press reset and have it contribute to the reset count. 

The repo at https://github.com/mattlazarowitz/mlaz_ESP8266_iot_sht30 uses this framework to create a device that uses MQTT as the means to send data to a server and an SHT-3x temperature and humidity sensor.

# improvements
I’m not sure it’s worth adding to this framework. The only thing I may add is OTA updates and I may experiment with the optional ability to send config changes via MQTT for devices that make use of that protocol. But I’m pretty happy with what I have been able to explore and implement with this framework.

# outro
To go back to the story, and the idea to think of this as a product. Would I make a product like this? Right now, I’d say no. ESP8266 security under Arduino is hard. I could not find a suitable HTTPS server, and there isn’t secure storage for the WiFi password for starters. 
If I did this again, I might skip the webpage system altogether in favor of something like SFTP and an app to create the JSON file and transmit it. I’d consider the idea of leaving a channel open for changing the config on the fly without a mode switch.
The mode selection by reset is not a great design. I devised it for ESP-01s compatibility but I switched over to a D1 mini clone so I could get more pins for a bigger sensor project. I’d dedicate some of those to an input to set the config mode behavior.

But in my opinion, this is better than the generic simple Arduino sample projects and brings together some concepts into a fairly capable system that demonstrates going from a bare, basic example of something to a project that adds a number of Quality of Life requirements to the project.   
