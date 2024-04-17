#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "DevConfigData.h"


// 
JsonDocument jsonConfig;

//need a better way to manage config keys
//these need to be part of the setup of a normal boot
//and the HTML requests. 
/*
ssid
pw
name

*/

bool DevConfigData::Begin (String filePath)
{
  configFilePath = filePath;
  return loadConfigData();
}

bool DevConfigData::loadConfigData()
{
  if (configFilePath.length() == 0) {
    Serial.println("devConfigData.loadConfigData: No config file set");
    return false; 
  }
  File configFile = LittleFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("devConfigData.loadConfigData: failed to read file");
    return false;
  }
  size_t size = configFile.size();
  //is this part even necessary?
  if (size > 4096) { //using 4K min alloc size for littleFS. Actual size should be smaller
    Serial.println("Data file size is too large");
    return false;
  }

  std::unique_ptr<char[]> configBuf(new char[size]);

  configFile.readBytes(configBuf.get(), size);
  configFile.close();
  auto error = deserializeJson(jsonConfig, configBuf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    Serial.println(error.f_str());
    return false;
  }
  return true;
}

// Saves the configuration to a file
bool DevConfigData::saveConfigData() {
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.remove(configFilePath)) {
    Serial.println("File deleted");
  }

  // Open file for writing
  File file = LittleFS.open(configFilePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }

  // Serialize JSON to file
  if (serializeJson(jsonConfig, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
  return true;
}

/*
//setters
void DevConfigData::setConfigSsid(const String & devSsid)
{
  configWiFiSsid = devSsid;
  return;
}
void DevConfigData::setConfigWifiPw(const String  & WiFiPw)
{
  configWifiPw = WiFiPw;
  return;
}
void DevConfigData::setConfigDevHostname (const String & devHostname)
{
  configDevHostname = devHostname;
  return;
}
void DevConfigData::setConfigFilePath (const String & devFilePath){
  configFilePath = devFilePath;
  return;
}

//getters
String DevConfigData::getConfigSsid()
{
  return configWiFiSsid;
}
//Really want a better way to handle this. 
String DevConfigData::getConfigWifiPw()
{
  return configWifiPw;
}
String DevConfigData::getConfigDevHostname()
{
  return configDevHostname;
}
String DevConfigData::getConfigFilePath()
{
  return configFilePath;
}

bool DevConfigData::isConfigLoaded()
{
  return configLoaded;
}
    
bool DevConfigData::clearConfig(){
  //clear local data storage
    configWiFiSsid = "";
    configWifiPw = "";
    configDevHostname = "";

  if (LittleFS.remove("config.json")) {
    Serial.println("File deleted");
    return true;
  } else {
    Serial.println("Delete failed");
    return false;
  }
}
*/