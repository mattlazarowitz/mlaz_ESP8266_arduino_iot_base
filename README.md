Inital 'draft' for an IOT device foundation.
This is a work in progress and more changes are planned.
This sketch relies on the following:
LittleFS
ESPAsyncWebServer
AsyncTCP
RTCMemory
ArduinoJson

After programing, use the LittleFS uploader from the Arduino IDE to send the contents of the data folder to the device.
You can optionally create a configuration file that cna be uploaded during this step to configure the device ahead of time.

The 'out of the box' experience/flow is as follows:
The device boots in softAP mode and starts a webserver. The wifi SSID is congig_<default ESP name> which is to indicate a device in config mode and avoid name conflicts.
Connecting to the webserver brings you to a page where you enter the WiFi infor for the target network and can set an optional hostname (my use case is to spot the DHCP leases mroe easilly).

After submitting the settings, saving them to a config file, and rebooting, the ESP8266 will reboot in 'device mode' where it uses the supplied WiFi credentials and this is where the device specefic stuff should go.

The device can be returned to its inital configuration state by resetting it 4 times in succession waiting about a half second between each press of the reset button.

Additonally, two resets will put the device in its config mode, but connected to the configured network. The intent is to update things like the hostname or MQTT details once implemented. 
Three resets will put the device back in config mode and run as a softAP. This is intededfor situations where WiFi settings have changed but other configuration info should be preserved.

This somewhat odd system was implemented to accomodate ESP-01 devices that don't have the pins to add a dedicated button.


TODO: Discuss structure of code

TODO: remove config class and use HTML callbacks to directly write the JSON fields. This improves the ability to configure the device since this intermediate class doesn't need to be modified.

TODO: investigate adding OTA capabilities. My past eperiments show that geting the devices to reflash them from my PC is a bit of a pain and I'd like to be able to program them in the field

TODO: Investigate the ability to send and recieve the config file over the network.

Long term TODO: Use MQTT to be able to set modes and maybe use it to set config values.
