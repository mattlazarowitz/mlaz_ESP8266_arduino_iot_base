#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include <RTCMemory.h>
#include <include/WiFiState.h> // WiFiState structure details

#include <ESP8266TimerInterrupt.h>

// Project specefic includes
#include "DevConfigData.h"
#include "HtmlRequests.h"

//Sketch specefic data types

//Data to be saved to the RTC RAM
//This holds Wifi state data and a count of "interrupted boots" 
//for temporary mode overrides
typedef struct {
  unsigned int unhandledResetCount;
  WiFiState state;
} devRtcData;

//used to direct behavior based on the state of the device.
enum devOpMode {
  staDevice, //regular mode. Device is in station mode, it do the normal device functions
  staConfig, //"station mode" meanin on the configured wifi network, but boots to server the configuration pages to allow config updates
  apConfig,  //AP mode config mode. Boot as an AP that can be connected to t in order to get to the config page that way. Config is not erased
  resetConfig, //erase the config settings, "factory reset"
  errorNoFs
};


//globals
AsyncWebServer server(80);

// Init ESP8266 timer 1
ESP8266Timer ITimer;

RTCMemory<devRtcData> rtcMemIface;
DevConfigData devConfig;

devOpMode BootMode;



//
// Timer ISR. 
// See ReadMe for design discussion.
// This timer interrupt acts as a one-shot to clear the reset count
// if the boot progresses far enough to count as booted.
//
void IRAM_ATTR TimerHandler()
{
  devRtcData* myRtcData = rtcMemIface.getData();
  myRtcData->unhandledResetCount = 0;
  rtcMemIface.save();
  //This may be bad, but it seems to work OK for now.
  timer1_disable();
}

//no wifi info, need to start in AP mode
void setupNewConfigMode()
{
  //setup softAP mode
  String configEspHostname = String("config_") + WiFi.hostname().c_str();
  WiFi.softAPConfig(IPAddress(192,168,30,1), IPAddress(0,0,0,0), IPAddress(255,255,255,0));
  Serial.print("AP hostname: ");
  Serial.println(configEspHostname);
  WiFi.softAP(configEspHostname);

  // setup HTTP server and the HTML requests
  registerHtmlInterfaces();
  server.begin();
}

//config mode requested by user but will try to be a device on provided wifi
void setupReconfigMode()
{
  //=================================
  //move all this to a wifo station mode func after testing?
  WiFi.hostname(static_cast<String>(jsonConfig["name"]).c_str());
  Serial.print("Connecting to ");
  Serial.println(static_cast<String>(jsonConfig["ssid"]));

  WiFi.mode(WIFI_STA);
  WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["pw"]));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //========================================================

  registerHtmlInterfaces();
  server.begin();
}


//regular mode
//this cleans up setupDevMode() nad isolates the wifi stuff so it can 
//be done at the appropriate point in the sensor init, sensor read, data sent sequence.
void DevModeWifi(devRtcData* data) {
  String SsidStr = (char*)data->state.state.fwconfig.ssid;
  if(SsidStr.equals(static_cast<String>(jsonConfig["ssid"]))){
    if (!WiFi.resumeFromShutdown(data->state)) {
      // Failed to restore state, do a regular connect.
      WiFi.persistent(false); 
      //invalidate the state data in case we fail a regular connect too.
      data->state.state.fwconfig.ssid[0] = 0;
      WiFi.hostname(static_cast<String>(jsonConfig["ssid"]));
      WiFi.mode(WIFI_STA);
      WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["pw"]));
    } 
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//may not need this. Here mostly for reference.
void devModeEnd(devRtcData* data) {
    if (data != nullptr) {
      WiFi.shutdown(data->state);
      rtcMemIface.save();
      delay (10);
    } else {
      WiFi.disconnect( true );
      delay(1);
    }
}


void setupDevMode()
{
  DevModeWifi(rtcMemIface.getData());
}

//
// There are some common steps needed by all boot modes. Those get performed here.
//
bool commonInit(){
  devRtcData* myRtcData = rtcMemIface.getData();
  ITimer.attachInterruptInterval(750000, TimerHandler);
  Serial.println("Mount LittleFS");
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    BootMode =  errorNoFs;
    return false;
  }
  if (devConfig.Begin("/config.json")){
    Serial.println ("config loaded");
    BootMode =  staDevice;
  }
  //There is an FS so that's OK, but no config.
  BootMode =  apConfig;
  //
  // Fetch data from RTC memory
  //
  if(!rtcMemIface.begin()){
    // probably the first boot after a power loss
    Serial.println("No RTC data");
    //need error recovery...
  }
  
  //increment the count and save back to RTC RAM
  Serial.print("reset count: ");
  Serial.println(myRtcData->unhandledResetCount);
  myRtcData->unhandledResetCount += 1;
  rtcMemIface.save();
  //now see if we hit any of the manual mode override thresholds
  switch (myRtcData->unhandledResetCount) {
    case 2:
      Serial.println("reconfig on configed network");
      if (BootMode == staDevice) {
        BootMode = staConfig;
      } else {
        //just go into normal config mode.
        BootMode = apConfig;
      }
      break;
    case 3:
      Serial.println("return to AP mode, keep config");
      BootMode = apConfig;
      break;
    case 4:
      Serial.println("\"factory reset\"");
      BootMode = resetConfig;
      break;
    default:
      // Maybe make this call the code to get config data and set mode here.
      // for now just set normal mode and let code below sort things out.
      Serial.println("normal boot");
      BootMode = staDevice;
      break;
  }
  return true;
}

void setup() {
  // I may want to make the mode specefic setups handled via a class factory.
  // Why? Just to practice it.
  //note that getting LittleFS running is mandadory for all modes and 
  //trying to load the config is common for all modes.
  //Note: Not sure how LittleFS impacts battery use.
  Serial.begin(115200);
  delay(20);
  Serial.println("Start setup");

  commonInit();

  switch (BootMode) {
    case staConfig:
      setupReconfigMode();
      break;
    case staDevice:
      setupDevMode();
      break;
    case apConfig:
      setupNewConfigMode();
      break;
    default:
      Serial.println("Invalid boot mode");
      break;

  }
}

void loop() {
// check BootMode and do the right loop required based on that.
}
