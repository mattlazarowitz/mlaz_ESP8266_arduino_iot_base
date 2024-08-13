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
#include "rtcInterface.hpp"


//
// Device mode worker function. 
// This is used to try and restore a saved WiFi connection.
// Performing this operation leads to faster wifi connections and reduced WiFi power draw.
//
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

//
// Setup() sub-function
// This is the vertion of the Setup() function that needs to be called when the 
// ESP8266 is operating in staDevice mode. 
// This is 
// 
void setupDevMode()
{
  DevModeWifi(rtcMemIface.getData());
}



void loopDevMode()
{

}