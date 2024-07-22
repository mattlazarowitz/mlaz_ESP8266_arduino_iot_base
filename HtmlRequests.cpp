//Moving HTML functions here to de-clutter the main sketch.
//maybe put this into the header file?

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "htmlRequests.h"
//#include "DevConfigData.h"
#include "configItems.h"

//best way to do this?
//extern DevConfigData devConfig;
extern AsyncWebServer server;
extern JsonDocument jsonConfig;


bool configSaved = false;


configItemData configItems [] = CONFIG_ITEMS;
//extern configItemData configItems [];
String reportFields;
//The complete list of config fields to be used for the %CONFIG_FIELDS% template
String configFields;

//
// This might be better in configItems.cpp
// but the data needed are globals here
//
//bool initValues (JsonDocument jsonConfig, configItemData *configItem) {
bool initValues () {
  Serial.println(F("init values"));
  //load up the JSON file and prepare to save it.
  for (int i = 0; i < std::size(configItems); i++) {
    Serial.print(F("loading [\""));
    Serial.print(configItems[i].key);
    Serial.print(F("\"]:[\""));  
    if (jsonConfig.containsKey(configItems[i].key)) {
      configItems[i].value = static_cast<String>(jsonConfig[configItems[i].key]);
      if (configItems[i].protect_pw) {
        Serial.print(F("<protected PW>"));
      } else {
        Serial.print(configItems[i].value);
      }
      
    } else {
      Serial.print(F("<not found>"));
    }
    Serial.println(F("\"]"));
  }
  return true;
}

bool valuesToJson () {
  Serial.println(F("valuesToJson"));
  for (int i = 0; i < std::size(configItems); i++) {
    jsonConfig[configItems[i].key] = configItems[i].value;
  }
  return saveConfigFile(CONFIG_FILE);
}


//
//    Webserver HTML template processor/callback
//
  
String processor(const String& var)
{   
  Serial.print(F("str processor: "));
  Serial.println(var);


  if (var == "CONFIG_SAVED") {
    if (configSaved) {
      return "configuration saved";
    }
  }

//switch to this once configs are done
  if (var == "CONFIG_FIELDS") {
    return configFields;
  }
  if (var == "REPORT_FIELDS"){
    return reportFields;
  }
  // The template item is likely a key.
  // Something like a hash table may have a O(1) while this loop is O(n)
  // But the loop is simpler and n is very small.
  //for (configItemData item : configItems) {
  for (int i = 0; i < std::size(configItems); i++) {
    //not any predefined fields, need to see if this is a user defined value
    //String retval = getItemValue(var, &item);
    //do this to end the loop as quickly as possible
    String retVal;
    //if (getItemValue(var, &item, retVal)) {
    if (getItemValue(var, &configItems[i], retVal)) {
      return retVal;
    }
  }
  return String();
}

void HandleConfigRequest(AsyncWebServerRequest *request) {
  String inputMessage;
  String inputParam;
  Serial.println(F("request_handler"));
  int params = request->params();

  // look through the config objects looking for the provided key
    //for (configItemData item : configItems) {
  for (int i = 0; i < std::size(configItems); i++) {
      handleFormResponse(&configItems[i],
      request);
      Serial.println(F("HandleConfigRequest: reading back data:"));
      Serial.print(configItems[i].key);
      Serial.print(F(":"));
      String Test = configItems[i].value;
      Serial.println(Test);

    }
  //request->send(LittleFS, "/index.htm", "text/html", false, processor);
  request->redirect("/");
}


void HandleSaveRequest(AsyncWebServerRequest *request) {
  Serial.println("do save stuff here");
  //configSaved = devConfig.saveConfigData();
  //configSaved = saveConfigFile(CONFIG_FILE);
  configSaved = valuesToJson();
  //configSaved = true;
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}

void HandleRebootRequest (AsyncWebServerRequest *request) {
  Serial.println(F("rebooting..."));
  request->send(200, "text/plain", "Rebooting...");
  delay (1000);
  ESP.restart();
  delay (1000);
}

void HandleClearRequest (AsyncWebServerRequest *request) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.print(F("Deleting config"));
  //TODO check return status
  //devConfig.clearConfig();
  jsonConfig.clear();
  for (int i = 0; i < std::size(configItems); i++) {
    configItems[i].value.remove(0,configItems[i].value.length());
  }
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}


void notFound(AsyncWebServerRequest *request) {

  //Serial.print(request);
  request->send(404, "text/plain", "Not found");
}


//rename this as HTML startup. Instantiate the config objects here and build the HTML that needs to be output
void registerHtmlInterfaces()
{
  //String tempStr1 = new String;
  //String tempStr2 = new String;
  Serial.println(F("registerHtmlInterfaces"));
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    //request->send_P(200, "text/html", index_html, processor);
    request->send(LittleFS, "/index.htm", "text/html", false, processor);
  });
  server.on("/config", HTTP_POST, HandleConfigRequest);
  server.on("/save", HTTP_POST, HandleSaveRequest);
  server.on("/reset", HTTP_POST, HandleClearRequest);
  server.on("/reboot", HTTP_POST, HandleRebootRequest);
  server.onNotFound(notFound);

  //build up our strings for the templates
  //they won't change so only do this once.
  //for (configItemData item : configItems) {
  for (int i = 0; i < std::size(configItems); i++) {
    //configFields = configFields + buildInputFormItem(&item);
    configFields = configFields + buildInputFormItem(&configItems[i]);
    //reportFields = reportFields + buildReportItem(&item);
    reportFields = reportFields + buildReportItem(&configItems[i]);
  }
  Serial.println(F("reporting built strings:"));
  Serial.println(configFields);
  Serial.println();
  Serial.println(reportFields);
  //configFields = tempStr1;
  //reportFields = tempStr2;
  //Move loaded data into the values in configItems
  initValues();
}