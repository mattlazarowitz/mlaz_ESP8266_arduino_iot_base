#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "DevConfigData.h"




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
  JsonDocument configData;
  auto error = deserializeJson(configData, configBuf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    Serial.println(error.f_str());
    return false;
  }
  //I should have my object now
  String configWiFiSsid = configData["ssid"];
  setConfigSsid(configData["ssid"]);
  String configWifiPw = configData["pw"];
  setConfigWifiPw(configData["pw"]);

  String configDevHostname = configData["name"];
  setConfigDevHostname(configData["name"]);

  Serial.println("SSID " + configWiFiSsid + ", PW " + configWifiPw + ", Hostname " + configDevHostname);
  return true;
}

bool DevConfigData::saveConfigData()
{
  String settings = "{\"ssid\":\"" + configWiFiSsid + "\",\"pw\":\"" + configWifiPw + "\",\"name\":\"" + configDevHostname + "\"}";
  Serial.println(settings);
  delay(1000);

  const int length = settings.length();
  std::unique_ptr<char[]> configBuf(new char[length]);
  //char* char_array = new char[length + 1];
  strcpy(configBuf.get(), settings.c_str()); 
  //writeFile("/config.json",configBuf.get());

  Serial.println("Writing file");
  File file = LittleFS.open(configFilePath, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  bool writeStatus = file.print(settings);
  if (writeStatus) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  delay(1000);  // Make sure the CREATE and LASTWRITE times are different
  file.close();
  return writeStatus;
}



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