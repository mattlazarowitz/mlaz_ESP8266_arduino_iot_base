//Moving HTML functions here to de-clutter the main sketch.
//maybe put this into the header file?

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "DevConfigData.h"

//best way to do this?
extern DevConfigData devConfig;
extern AsyncWebServer server;



bool configSaved = false;





//
//    Webserverr HTML template processor/callback
//

String processor(const String& var)
{
  Serial.println("str processor:");
  Serial.println(var);

  if (var == "SSID_PLACEHOLDER") {
    if (jsonConfig.containsKey("ssid")) {
      Serial.print("returning ");
      Serial.println(static_cast<String>(jsonConfig["ssid"]));
      return static_cast<String>(jsonConfig["ssid"]);
    } 
  }
  if (var == "ESP_NAME_PLACEHOLDER") {
    if (jsonConfig.containsKey("name")) {
      Serial.print("returning ");
      Serial.println(static_cast<String>(jsonConfig["name"]));
      return static_cast<String>(jsonConfig["name"]);
    }
  }
  if (var == "BLANK_WIFI_PASSWORD") {
    Serial.print("returning ");
    if (jsonConfig.containsKey("pw")) {
      if(static_cast<String>(jsonConfig["pw"]).length() > 0) {
        return "PW set";
      }
    }
  }
  if (var == "CONFIG_SAVED") {
    if (configSaved) {
      return "configuration saved";
    }
  }

  return String();
}

void HandleConfigRequest(AsyncWebServerRequest *request) {
  String inputMessage;
  String inputParam;
  Serial.println("request_handler");
  int params = request->params();
  //https://forum.arduino.cc/t/espasyncwebserver-post-method-for-any-example-with-gui-working/963099/2
  //I like the loop structure there as it's reasonably generic.
  for (int i = 0; i < params; i++) {
    AsyncWebParameter* p = request->getParam(i);
    Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    //if (request->hasParam("SsidInput")) {
    //  Serial.println("SsidInput");
    //  inputMessage = request->getParam("SsidInput")->value();
    //  configWiFiSsid = inputMessage;
    //}
  }
  if (request->hasParam("SsidInput", true)) {
    configSaved = false;
    Serial.println("SsidInput");
    inputMessage = request->getParam("SsidInput", true)->value();
    if (inputMessage.length() > 0) {
      //configWiFiSsid = inputMessage;
      //devConfig.setConfigSsid(inputMessage);
      jsonConfig["ssid"] = inputMessage;
    }
  }
  // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
  if (request->hasParam("PwInput", true)) {
    configSaved = false;
    Serial.println("PwInput");
    inputMessage = request->getParam("PwInput", true)->value();
    //configWiFiAuth = inputMessage;
    //devConfig.setConfigWifiPw(inputMessage);
    jsonConfig["pw"] = inputMessage;
  }
  // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
  if (request->hasParam("HostName", true)) {
    configSaved = false;
    Serial.println("HostName");
    inputMessage = request->getParam("HostName", true)->value();
    if (inputMessage.length() > 0) {
      Serial.println("setting configEspHostname");
      //configEspHostname = inputMessage;
      //devConfig.setConfigDevHostname(inputMessage);
      jsonConfig["name"] = inputMessage;
    }
  }
  //request->send(LittleFS, "/index.htm", "text/html", false, processor);
  request->redirect("/");
}




void HandleSaveRequest(AsyncWebServerRequest *request) {
  Serial.println("do save stuff here");
  configSaved = devConfig.saveConfigData();
  //configSaved = true;
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}

void HandleRebootRequest (AsyncWebServerRequest *request) {
  Serial.println("rebooting...");
  request->send(200, "text/plain", "Rebooting...");
  delay (1000);
  ESP.restart();
  delay (1000);
}

void HandleClearRequest (AsyncWebServerRequest *request) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.printf("Deleting config");
  //TODO check return status
  //devConfig.clearConfig();
  jsonConfig.clear();
  //TODO:make sure config gets saved or config file is deleted.
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}


void notFound(AsyncWebServerRequest *request) {

  //Serial.print(request);
  request->send(404, "text/plain", "Not found");
}



void registerHtmlInterfaces()
{
  Serial.println("registerHtmlInterfaces");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    //request->send_P(200, "text/html", index_html, processor);
    request->send(LittleFS, "/index.htm", "text/html", false, processor);
  });
  server.on("/config", HTTP_POST, HandleConfigRequest);
  server.on("/save", HTTP_POST, HandleSaveRequest);
  server.on("/reset", HTTP_POST, HandleClearRequest);
  server.on("/reboot", HTTP_POST, HandleRebootRequest);
  server.onNotFound(notFound);
}