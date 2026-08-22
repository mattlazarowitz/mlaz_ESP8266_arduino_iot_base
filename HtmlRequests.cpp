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
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>


#include "configItems.hpp"
#include "HtmlRequests.hpp"

extern AsyncWebServer server;

//
// Outcome of the most recent /save request. See saveStatus enum docs in
// HtmlRequests.hpp. Defaults to idle so a fresh boot's config page does
// not display a stale banner.
//
saveStatus lastSaveStatus = saveStatus::idle;


String reportFields;
//The complete list of config fields to be used for the %CONFIG_FIELDS% template
String configFields;

// TODO: rename this symbol once all functional config code is fully
// encapsulated in the configurationItems class so the global and the
// type don't share a near-identical name.
configurationItems configItems;

//
//    Webserver HTML template processor/callback
//

String processor(const String& var) {
  String retVal;
  Serial.print(F("str processor: "));
  Serial.println(var);

  if (var == "CONFIG_SAVED") {
    switch (lastSaveStatus) {
      case saveStatus::saved:
        return F("<p style=\"color:green\">Configuration saved.</p>");
      case saveStatus::failed:
        return F("<p style=\"color:red\">"
                 "Failed to save the configuration. The settings shown "
                 "above were not written to flash."
                 "</p>");
      case saveStatus::idle:
      default:
        return String();
    }
  }
  if (var == "CONFIG_FIELDS") {
    return configFields;
  }
  if (var == "REPORT_FIELDS"){
    return reportFields;
  }
  if (var == "CONFIG_STATUS") {
    // Only the corrupt case surfaces UI. loaded and missing both render
    // empty so the page layout is unchanged on a normal device.
    if (lastConfigLoadResult == loadConfigResult::corrupt) {
      return F("<p style=\"color:red\">"
               "The previous configuration file was corrupted and has been "
               "erased. Please re-enter your settings."
               "</p>");
    }
    return String();
  }
  if (configItems.getItemValue(var, retVal)) {
    return retVal;
  }
  return String();
}

void HandleConfigRequest(AsyncWebServerRequest *request) {
  Serial.println(F("request_handler"));
  // The configurationItems class walks the request itself; this handler
  // just forwards the form data through and returns the user to the
  // top-level page.
  configItems.saveResponseValues(request);
  request->redirect("/");
}


void HandleSaveRequest(AsyncWebServerRequest *request) {
  Serial.println("do save stuff here");
  configItems.dumpToJson(jsonConfig);
  if (configItems.isEmpty() || jsonConfig.isNull()) {
    eraseConfig(CONFIG_FILE);
    // No config persisted is functionally the same as not having one; this
    // also clears any leftover "corrupt" banner from a prior load attempt.
    lastConfigLoadResult = loadConfigResult::missing;
    // Erasing in response to a save with no fields populated is the
    // user's expressed intent; treat the operation as a successful save.
    lastSaveStatus = saveStatus::saved;
  } else {
    configItems.dumpToJson(jsonConfig);
    if (saveConfigFile(CONFIG_FILE)) {
      // Successful write supersedes whatever the prior load saw, including
      // any earlier corrupt-file recovery state. Subsequent renders of the
      // config page should not show the recovery banner.
      lastConfigLoadResult = loadConfigResult::loaded;
      lastSaveStatus = saveStatus::saved;
    } else {
      // Write failed. Leave lastConfigLoadResult alone so any previous
      // corrupt-state banner is preserved (the user is going to need to
      // try again either way), and surface the failure on the page so
      // the user does not assume their settings were persisted.
      lastSaveStatus = saveStatus::failed;
    }
  }
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}

void HandleRebootRequest (AsyncWebServerRequest *request) {
  Serial.println(F("rebooting..."));
  request->send(200, "text/plain", "Rebooting...");
  ESP.restart();
}

void HandleClearRequest (AsyncWebServerRequest *request) {
  // Clear in-memory config; the persisted file is left alone here and
  // is rewritten (or erased) the next time the user submits a save.
  // TODO: factor this into configurationItems so the global jsonConfig
  // does not need to be touched directly from the HTTP layer.
  Serial.print(F("Deleting config"));
  jsonConfig.clear();
  configItems.clearValues();
  // User-initiated clear: explicitly drop any prior corrupt-state banner
  // and any stale save-status banner, since we are starting a fresh
  // configuration cycle.
  lastConfigLoadResult = loadConfigResult::missing;
  lastSaveStatus = saveStatus::idle;
  request->send(LittleFS, "/index.htm", "text/html", false, processor);
}


void notFound(AsyncWebServerRequest *request) {

  //Serial.print(request);
  request->send(404, "text/plain", "Not found");
}


//
// Register the HTTP routes the config web UI exposes, populate the
// configurationItems instance from the loaded JSON config, and build the
// static template fragments (configFields, reportFields) that the
// processor() callback substitutes into index.htm. Called once per boot
// from whichever mode wants the config server up (apConfig or staConfig).
//
void registerHtmlInterfaces()
{
  Serial.println(F("registerHtmlInterfaces"));
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/index.htm", "text/html", false, processor);
  });
  server.on("/config", HTTP_POST, HandleConfigRequest);
  server.on("/save", HTTP_POST, HandleSaveRequest);
  server.on("/reset", HTTP_POST, HandleClearRequest);
  server.on("/reboot", HTTP_POST, HandleRebootRequest);
  server.onNotFound(notFound);

  //Init the config class
  configItems.LoadValues(jsonConfig);
  //build up our strings for the templates
  //they won't change so only do this once.
  configItems.buildInputFormEntries(configFields);
  configItems.buildReportEntries(reportFields);
  Serial.println(F("Input fields:"));
  Serial.println(configFields);
  Serial.println();
  Serial.println(F("Report fields:"));
  Serial.println(reportFields);

}