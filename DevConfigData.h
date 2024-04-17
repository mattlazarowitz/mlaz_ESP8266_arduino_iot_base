
#include <ArduinoJson.h>

extern JsonDocument jsonConfig;

/*
 * Initial "get it done" class for config data.
 * The idea is for the data to live in a JSON file in the filesystem portion of the 
 * flash layout.
 * 
 * The ESP8266 docs say to use LittleFS so I haven't made this to be more filesystem
 * agnostic in a manner similar to the ESPAsyncWebServer library. That is a project for future me.
 * 
 * I did not intend to follow the library format but after realizing I have the same scope concerns
 * as a library, I'm following a similar pattern with a begin method because I need this class to be
 * global but depend on other libraries to be started first.
 * 
 */
class DevConfigData {
  protected:
    //can this be turned into a vactor of key-value pairs to make this more flexible?
    String configWiFiSsid;
    String configWifiPw;
    String configDevHostname;
    String configFilePath;
    bool configLoaded;


  public:
    bool Begin(String filePath);
    bool loadConfigData();
    bool saveConfigData();
    bool clearConfig();

    //setters
    void setConfigSsid(const String & devSsid);
    void setConfigWifiPw(const String & devSsid);
    void setConfigDevHostname (const String & devHostname);
    void setConfigFilePath (const String & devFilePath);

    //getters
    String getConfigSsid();
    //Really want a better way to handle this. 
    String getConfigWifiPw();
    String getConfigDevHostname();
    String getConfigFilePath();

    bool isConfigLoaded();
    
};
