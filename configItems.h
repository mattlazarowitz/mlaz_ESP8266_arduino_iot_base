#ifndef CONFIG_ITEMS_H_
#define CONFIG_ITEMS_H_

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"

extern JsonDocument jsonConfig;

struct configItemData_old {
    const String displayName;
    const String key;
    const boolean protect_pw;
    const int maxLength;
    String value;
};

struct configItemData {
    const String displayName;
    const String key;
    const boolean protect_pw;
    const int maxLength;
    String value;
};

String buildInputFormItem(configItemData *configItem);
String buildReportItem(configItemData *configItem);
int handleFormResponse(configItemData *configItem, AsyncWebServerRequest *request);
bool getItemValue(String templateVar, configItemData *configItem, String& valueString);

bool loadConfigFile(String configFileLoc);
bool saveConfigFile(String configFileLoc);
bool eraseConfig (String configFileLoc);

//extern configItemData configItems[];

//I dislike this design pattern in part because the line continuations are ugly.
//But I haven't been able to find the 'correct' OOP pattern.
//Perhaps employing a variation of Cunningham's Law will yield something.

//Add an entry here for it to pop up in the HTML config page.
#define CONFIG_ITEMS \
{ \
    { \
        "Device host name", \
        "hostname", \
        false, \
        32 \
    }, \
    { \
        "WiFi SSID", \
        "ssid", \
        false, \
        32 \
    }, \
    { \
        "WiFi Password", \
        "WiFiPw", \
        true, \
        64 \
    } \
}

#endif