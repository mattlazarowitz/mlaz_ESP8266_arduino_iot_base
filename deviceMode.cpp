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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h> //needed by configItems.hpp

#include <include/WiFiState.h>
#include <RTCMemory.h>

#include "configItems.hpp"
#include "rtcInterface.hpp"
#include "backoff.hpp"

//
// Device mode worker function. 
// This is used to try and restore a saved WiFi connection.
// Performing this operation leads to faster wifi connections and reduced WiFi power draw.
//
// Reconnect handling: we deliberately do not register a manual
// onStationModeDisconnected handler here. The ESP8266 SDK's own
// auto-reconnect (enabled explicitly below) already retries on drop, and
// stacking a second hand-rolled retry on top of it leads to the radio
// being hammered with redundant WiFi.begin() calls during a sustained
// outage. If we later need disconnect-event-driven backoff, the right
// place for it is the same state machine that owns the boot-time
// progressive-sleep retry, so both paths share one policy.
//
void DevModeWifi(devRtcData* data) {
  String SsidStr = (char*)data->state.state.fwconfig.ssid;
  WiFi.setAutoReconnect(true);
  if(SsidStr.equals(static_cast<String>(jsonConfig["ssid"]))){
    if (!WiFi.resumeFromShutdown(data->state)) {
      // Failed to restore state, do a regular connect.
      WiFi.persistent(false); 
      //invalidate the state data in case we fail a regular connect too.
      data->state.state.fwconfig.ssid[0] = 0;
      WiFi.hostname(static_cast<String>(jsonConfig["hostname"]));
      WiFi.mode(WIFI_STA);
      WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["WiFiPw"]));
    }
  } else {
    WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["WiFiPw"]));
  }

  // Bounded connect attempt. An infinite spin here means an outage at
  // boot hangs the device forever, blocking the backoff / sleep path.
  const unsigned long connectTimeoutMs = 30000;
  const unsigned long connectStartMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStartMs >= connectTimeoutMs) {
      Serial.println();
      Serial.println(F("WiFi connect timed out; entering backoff sleep."));
      // Hand off to the progressive-backoff state machine. This bumps
      // the RTC-stored backoff level (capped) and deep-sleeps for the
      // corresponding duration. The reset counter is cleared inside,
      // so the resulting wake is not interpreted as a user-press for
      // the multi-press mode-change signal. Does not return.
      backoffMarkFailureAndSleep();
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
  Serial.println(WiFi.localIP());
}

//
// Setup() sub-function
// This is the version of the Setup() function that needs to be called when the 
// ESP8266 is operating in staDevice mode. 
// The end-of-cycle Wi-Fi state save + deep sleep that earlier scaffolding
// gestured at is now provided by cleanDeepSleep() / backoffMarkFailureAndSleep()
// in backoff.cpp; project code in loopDevMode() calls those.
// 
void setupDevMode()
{
  DevModeWifi(rtcMemIface.getData());
}



void loopDevMode()
{

}