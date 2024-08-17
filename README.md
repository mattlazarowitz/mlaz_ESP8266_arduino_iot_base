**Current State**
Consider this as a near final beta.
This version is a refactor of code that passed some basic tests.

Problem: There is a lack of examples of IOT devices that use the ESP-8266, use the Arduino framework, and are runtime configurable.

Why ESP-8266?
  - Cheap
  - Many 'flavors' to choose from
  - Good community support
  
Why Arduino?
  - Community
  - Many libraries to utilize
  - more flexibility than options like ESPHome
  
Why runtime configurable?
  - No need to recompile and flash to update wifi or other infrastructure changes
  
Desired features:
  - Runtime configurable
    - The configuration should be able to be set wirelessly without the use of the Arduino IDE
    - Any device such as a PC, tablet, or phone should be able to configure the device
    - No app or additional software should be needed to configure the device
  - The configuration does not need to be re-entered after a power loss or reset
    - The configuration is stored on device in non-volatile storage
  - Minimal manual data management of stored parameters
    - The configuration will be stored in a manner that is data agnostic
    - The data will be stored in a format that is not dependent on data size
  - The ability to update configuration after setting it
    - The device needs to accept an external input requesting the configuration interface
    - The ability to reconfigure wifi network parameters must be supported
  - The ability to clear stored configuration data without the use of the runtime interface
    - The user should be able to supply a signal that indicates the device must clear stored configuration values
  - The ability to support multiple ESP-8266 board types
    - Current targets are ESP-01s, NodeMCU, and Wemos D1 mini
  - Minimize code changes to support different combinations of configuration parameters
    - Configuration items should be encapsulated in a way that minimizes the code that needs to be added or changed when a new configuration parameter is needed.
  - Optional, be battery powered
    - Whenever possible, use methods that are known to save power
    
To facilitate this the device will do the following:
  - Support AP and station WiFi modes
  - Use an HTTP server to serve an HTML page for configuration
  - Store configuration data in JSON format as key-value pairs
  - Store JSON and HTML data using the LittleFS file system
  - Use the reset button to signal mode change requests
  - Make use of the template processing capability in the ESPAsyncWebServer
  - Define the options for the HTML form in a header file
  - Optional for power: Use wifi state saving and restoring to minimize radio power usage
  - Optional for power: Use the deep sleep capability of the ESP-8266
  - Optional for power: Infrastructure issue detection with longer sleep cycles
    
    

When the ESP boots and the Arduino framework starts, the setup function will check for a predetermined config file name. 
If it cannot find the file, it will come up in AP mode along with a basic DHCP server to that something with a web browser can connect to the device.
The configuration is entered via the browser and once saved, the system will come back up in a non-interactive device mode. 

The user will use multiple presses of the reset button to signal a request for mode changes.
Multiple presses of the reset button are used to signal mode change requests. 
A 'double reset' indicates a request to remain in station mode but start the web server on the IP granted to it by DHCP.
A 'triple press' indicated a request for the device to enter AP mode but retain all configured settings.
A 'quadrupal press' indicated the device should erase the configuration data.
To facilitate multiple press detection, the device will increment a counter with each boot. The count is stored in RTC RAM to persist across resets.
The count will be cleared after some delay in normal operation. The user has a time window before the count is cleared to hit the reset button for it to be logged.
Current experiments have used a timer ISR to handle the process of clearing the count as setup executes too quickly for a system where the count is reset at the end of the function.
More experimentation with a device that utilizes deep sleep is needed to see if there will be sufficient time with that model. 

Prior lessons concerning WiFi state saving and restoring will be employed to minimize the time the radio is powered on. 
Additionally the sketch implementation will gather data from the sensors before turning the radio on to transmit it in an effort to further reduce power.
Additional profiling will be needed to see if the use of LittleFS has a noticeable impact on power usage or if there is an alternative non-volatile data store that may be more power efficient.

How to use:
Open configItems.hpp
Scroll to the end of the file. 
Look for the vector configItems in the private data of the configurationItems class.
Add and or remove configItemData entries as needed.
Build, flash, and check if the served HTML configuration is correct.

Possible big issues:
Lack of https - there really isn't anything available for Arduino
Password security - After research, there isn't really a great I can find to hande this. On device encrption means needing an on device key which just adds a step.
                    Transmitting passwords in general is an issue. This is a subject that requires more research.
Future work:
More clean up
Look into making the config items class more generalized with the goal of being able to create more HTML entries than just text input fields.
Consider alternative methods to handle this
  - Phone based app configuration
    - Idea: construct config file on phone and send via something like sftp