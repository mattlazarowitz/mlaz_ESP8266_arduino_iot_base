/*
MIT License

Copyright (c) 2024 Matthew Lazarowitz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**/
#include "configItems.hpp"
#include "HtmlRequests.hpp"
#include <include/WiFiState.h>
#include <ESP8266TimerInterrupt.h>
#include <RTCMemory.h>

//Sketch specific data types

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
  staConfig, //"station mode" meaning on the configured wifi network, but boots to server the configuration pages to allow config updates
  apConfig,  //AP mode config mode. Boot as an AP that can be connected to t in order to get to the config page that way. Config is not erased
  resetConfig, //erase the config settings, "factory reset"
  errorNoFs
};


//globals
AsyncWebServer server(80);

// Init ESP8266 timer 1
ESP8266Timer ITimer;

RTCMemory<devRtcData> rtcMemIface;

devOpMode BootMode;

//
// Simple debug function to convert the boot mode into a string
//
String bootModeToStr(devOpMode BootMode) {
  switch (BootMode) {
    case staDevice:
      return "staDevice";
    case staConfig:
      return "staConfig";
    case apConfig:
      return "apConfig";
    case resetConfig:
      return "resetConfig";
    case errorNoFs:
      return "errorNoFs";
    default:
      break;
  }
  return "invalid boot mode";
}

//
// Timer ISR. 
// Used for boot mode change command.
// Resets withon a predetermined time window are counted and used to determine a user commanded boot mode change. 
// But if the count is reset at the end of Setup(), it's very hard for a human to use this functionality.
// So this timer ISR creates a 750ms window instead. 
// In battery powered device mode, this could become an issue if the device resets itself too quickly.
// In that case, it may be better to add the reset functionality right before the reset or deep sleep command.
// The process of reading a sensor, restoring the WiFi connection, and reporting the data should hopefully take 
// long enough to allow for reliable reset detection.
//
void IRAM_ATTR TimerHandler()
{
  devRtcData* myRtcData = rtcMemIface.getData();
  myRtcData->unhandledResetCount = 0;
  rtcMemIface.save();
  //This may be bad, but it seems to work OK for now.
  timer1_disable();
}

//
// Called from Setup() after the boot mode is determined.
// 
//
void setupApConfigMode()
{
  //setup softAP mode
  String configEspHostname = String("config_") + WiFi.hostname().c_str();
  WiFi.softAPConfig(IPAddress(192,168,30,1), IPAddress(0,0,0,0), IPAddress(255,255,255,0));
  Serial.print(F("AP hostname: "));
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
  //move all this to a wifi station mode func after testing?
  WiFi.hostname(static_cast<String>(jsonConfig["name"]).c_str());
  Serial.print(F("Connecting to "));
  Serial.println(static_cast<String>(jsonConfig["ssid"]));

  WiFi.mode(WIFI_STA);
  WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["pw"]));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
  Serial.println(WiFi.localIP());
  //========================================================

  registerHtmlInterfaces();
  server.begin();
}


//regular mode
//this cleans up setupDevMode() and isolates the wifi stuff so it can 
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

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
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
  //devRtcData* myRtcData = rtcMemIface.getData();
  devRtcData* myRtcData = nullptr;
  bool rtcInit;
  ITimer.attachInterruptInterval(750000, TimerHandler);
  Serial.println(F("Mount LittleFS"));
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed"));
    BootMode =  errorNoFs;
    return false;
  }

  if(loadConfigFile(CONFIG_FILE)) {
    Serial.println (F("config loaded"));
    BootMode =  staDevice; 
  } else {
    //There is an FS so that's OK, but no config.
    BootMode =  apConfig;
  }


  //
  // Fetch data from RTC memory
  //
  rtcInit = rtcMemIface.begin();
  if(!rtcInit){
    // probably the first boot after a power loss
    Serial.println(F("No RTC data"));
    // often happens if the CRC for the RTC RAM fails which is expected on a first boot. 
    // System tries to restore from flash (which we don't use for this).
    // If that fails, it does a reset of the data area which is what we want. 
    // We can either reset or try tgoe begin again. 
    // Go ahead and try to begin again. IF that fails, I think the best course of action is an
    // error 'halt' and blink the onboard LED.
    if (rtcMemIface.begin()){
      myRtcData = rtcMemIface.getData();
    }
  } else {
    Serial.println(F("reading RTC data"));
    myRtcData = rtcMemIface.getData();
  }
  if (myRtcData != nullptr) {
    //increment the count and save back to RTC RAM
    Serial.print(F("reset count: "));
    Serial.println(myRtcData->unhandledResetCount);
    myRtcData->unhandledResetCount += 1;
    rtcMemIface.save();
    //now see if we hit any of the manual mode override thresholds
    switch (myRtcData->unhandledResetCount) {
      case 2:
        Serial.println(F("reconfig on configed network"));
        if (BootMode == staDevice) {
          BootMode = staConfig;
        } else {
          //just go into normal config mode.
          BootMode = apConfig;
        }
        blinkLed(2);
        break;
      case 3:
        Serial.println("return to AP mode, keep config");
        BootMode = apConfig;
        blinkLed(3);
        break;
      case 4:
        Serial.println(F("\"factory reset\""));
        BootMode = resetConfig;
        break;
      default:
        // Maybe make this call the code to get config data and set mode here.
        // for now just set normal mode and let code below sort things out.
        Serial.println(F("no override"));
        Serial.print(F("Boot mode: "));
        Serial.println(bootModeToStr(BootMode));
        //BootMode = staDevice;
        blinkLed(4);
        break;
    }
  }
  return true;
}
void blinkLed(int blinks) {
  for (int i = 0; i < blinks;i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
}
void setup() {
  // I may want to make the mode specefic setups handled via a class factory.
  // Why? Just to practice it.
  //note that getting LittleFS running is mandadory for all modes and 
  //trying to load the config is common for all modes.
  //Note: Not sure how LittleFS impacts battery use.
  Serial.begin(115200);
  delay(20);
  Serial.println(F("Start setup"));
  pinMode(LED_BUILTIN, OUTPUT);

  commonInit();

  switch (BootMode) {
    case staConfig:
      setupReconfigMode();
      break;
    case staDevice:
      setupDevMode();
      break;
    case apConfig:
      setupApConfigMode();
      break;
    case resetConfig:
      //devConfig.clearConfig();
      eraseConfig(CONFIG_FILE);
      delay (1000);
      ESP.restart();
      delay (1000);
    default:
      Serial.println(F("Invalid boot mode"));
      break;

  }
}

void loop() {
// check BootMode and do the right loop required based on that.
}
