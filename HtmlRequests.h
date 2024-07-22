#ifndef HTML_REQUESTS_H_
#define HTML_REQUESTS_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include <RTCMemory.h>
#include <include/WiFiState.h> // WiFiState structure details

#include <ESP8266TimerInterrupt.h>



String processor(const String& var);
void HandleConfigRequest(AsyncWebServerRequest *request);
void HandleSaveRequest(AsyncWebServerRequest *request);
void HandleRebootRequest (AsyncWebServerRequest *request);
void HandleClearRequest (AsyncWebServerRequest *request);
void notFound(AsyncWebServerRequest *request);
void registerHtmlInterfaces();

#endif