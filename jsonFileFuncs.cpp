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
#include <LittleFS.h>

#include "configItems.hpp"



JsonDocument jsonConfig;

//
// Tracks the outcome of the most recent loadConfigFile() call. Read by
// the HTML template processor to decide whether to surface a corrupt-
// config recovery banner. Default of `missing` means: until the loader
// runs, there is by definition no known config, and no banner is shown.
//
loadConfigResult lastConfigLoadResult = loadConfigResult::missing;

//
// loadConfigFile
// Attempt to read the provided filename and load the JSON data into
// jsonConfig. The return value (and the lastConfigLoadResult global it
// updates as a side effect) distinguishes three outcomes:
//
//   loaded  - jsonConfig is populated and ready to use.
//   missing - the file does not exist on the FS. Treat as "unconfigured
//             device" rather than as an error.
//   corrupt - the file exists but cannot be opened, is unreasonably
//             large, or fails JSON parsing. The caller is responsible
//             for erasing it; this function deliberately does not erase
//             so the caller can log/inspect first.
//
loadConfigResult loadConfigFile(String configFileLoc)
{
  Serial.println(F("Loading configuration"));
  if (configFileLoc.length() == 0) {
    // Programmer error rather than a real on-device condition, but treat
    // as "no config to find" so commonInit's apConfig fallback still works.
    Serial.println("devConfigData.loadConfigData: No config file set");
    lastConfigLoadResult = loadConfigResult::missing;
    return lastConfigLoadResult;
  }
  if (!LittleFS.exists(configFileLoc)) {
    lastConfigLoadResult = loadConfigResult::missing;
    return lastConfigLoadResult;
  }
  File configFile = LittleFS.open(configFileLoc, "r");
  if (!configFile) {
    Serial.println(F("loadConfigData: failed to read file"));
    // The file exists per LittleFS.exists() but the FS won't open it.
    // That's not "missing", it's a corrupted-state signal.
    lastConfigLoadResult = loadConfigResult::corrupt;
    return lastConfigLoadResult;
  }
  size_t size = configFile.size();
  // Sanity bound: a config that exceeds the LittleFS minimum allocation
  // size for our setup is almost certainly garbage rather than a real
  // payload. Treat it as corrupt rather than attempting to parse it.
  if (size > 4096) {
    Serial.println(F("Data file size is too large"));
    configFile.close();
    lastConfigLoadResult = loadConfigResult::corrupt;
    return lastConfigLoadResult;
  }
  auto error = deserializeJson(jsonConfig, configFile);
  configFile.close();
  if (error) {
    Serial.println(F("Failed to parse config file"));
    Serial.println(error.f_str());
    lastConfigLoadResult = loadConfigResult::corrupt;
    return lastConfigLoadResult;
  }
  lastConfigLoadResult = loadConfigResult::loaded;
  return lastConfigLoadResult;
}

//
// saveConfigFile
// Persist jsonConfig as JSON to the provided file path on LittleFS.
//
// Returns true only if the entire write completed cleanly. Returns
// false if the file cannot be opened for writing, or if serialization
// produces zero bytes; in the latter case the partial file is removed
// before returning so disk state matches the reported result and the
// next boot does not have to disambiguate between "written" and
// "partially written" via the corrupt-config recovery path.
//
bool saveConfigFile(String configFileLoc) {
  Serial.println(F("saveConfigFile"));
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.remove(configFileLoc)) {
    Serial.println(F("File deleted"));
  }

  File file = LittleFS.open(configFileLoc, "w");
  if (!file) {
    Serial.println(F("Failed to open file for writing"));
    return false;
  }

  const size_t bytesWritten = serializeJson(jsonConfig, file);
  file.close();
  if (bytesWritten == 0) {
    Serial.println(F("Failed to write config: serialize returned 0 bytes"));
    // Do not leave a half-written file. The corrupt-config detection on
    // next boot would self-erase it anyway, but failing cleanly here
    // keeps in-session state consistent with the return value.
    LittleFS.remove(configFileLoc);
    return false;
  }
  Serial.println(F("Config saved"));
  return true;
}

//
// eraseConfig
// Attempt to erase the provided configuration file. Used as part of the
// factory-reset path and as the recovery action when a corrupt config
// is detected during boot.
//
// Returns true if the file is no longer present on the FS after the
// call (either because it was successfully removed, or because it was
// already absent). Returns false only if a remove was attempted and
// the FS reported failure.
//
// TODO: move this responsibility into configurationItems so the JSON
// model and its persistence are managed in one place.
//
bool eraseConfig (String configFileLoc) {
  Serial.print(F("eraseConfig: deleting "));
  Serial.println(configFileLoc);
  if (!LittleFS.exists(configFileLoc)) {
    // Already gone; treat as success since the desired post-condition
    // (file does not exist) is already true.
    return true;
  }
  const bool removed = LittleFS.remove(configFileLoc);
  // Brief settle delay: gives LittleFS time to finalize the underlying
  // flash block update before any caller that immediately reboots or
  // re-mounts the FS proceeds.
  delay(500);
  return removed;
}

