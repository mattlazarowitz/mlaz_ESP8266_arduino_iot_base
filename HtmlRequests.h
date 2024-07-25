#ifndef HTML_REQUESTS_H_
#define HTML_REQUESTS_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

String processor(const String& var);
void HandleConfigRequest(AsyncWebServerRequest *request);
void HandleSaveRequest(AsyncWebServerRequest *request);
void HandleRebootRequest (AsyncWebServerRequest *request);
void HandleClearRequest (AsyncWebServerRequest *request);
void notFound(AsyncWebServerRequest *request);
void registerHtmlInterfaces();

#endif