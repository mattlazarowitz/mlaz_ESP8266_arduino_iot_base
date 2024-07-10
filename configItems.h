#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

struct configItemData {
    String displayName;
    String key;
    boolean protect_pw;
    int maxLength;
};

String buildInputFormItem(configItemData *configItem);
String buildReportItem(configItemData *configItem);
int handleFormResponse(configItemData *configItem, JsonDocument *jsonConfig, AsyncWebServerRequest *request);
char* getItemValue(String templateVar, configItemData *configItem, JsonDocument *jsonConfig);

//extern configItemData configItems[];

// I dislike this design pattern in part because the line continuations are ugly.
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
