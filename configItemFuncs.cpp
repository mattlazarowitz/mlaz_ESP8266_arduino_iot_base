#include "configItems.h"

JsonDocument jsonConfig;

//
// buildInputFormItem
// HTML input is handled via a form with multiple fields. 
// This adds a field for the form section of the HTML as a table row.
//
// 1) Check if the field is for a protected item like a password.
// 2) If not, build a row where the input type is text.
// 3) If it is, build a row where the input type is a password.
// 4) Return the string for the row to the caller.
//
// Parameter: *configItem - A pointer to a configItemData data structure.
//    The structure contains the data needed to build the row.
//
// Returns HTLM string for a table row. 
//
String buildInputFormItem(configItemData *configItem){
    //<tr><td>{prettyName} <td><input type="text" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
    // This could probably be done more 'cleanly' in terms of syntax with sprint_f but the manual buffer managment
    // takes away from that. 
    // So while String concatination is syntactically messier, it seems to be the cleaner design.
    if (!configItem->protect_pw) {
        return "<tr><td>" +
          configItem->displayName +
          " <td><input type=\"text\" name=\"" +
          configItem->key +
          "\" maxlength=\"" +
          String(configItem->maxLength) +
          "\" placeholder=\"%" +
          configItem->key +
          "%\"><br>";
    }
    
    //<tr><td>{prettyName} <td><input type="password" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
      return "<tr><td>" +
        configItem->displayName +
        " <td><input type=\"password\" name=\"" +
        configItem->key +
        "\" maxlength=\"" +
        String(configItem->maxLength) +
        "\" placeholder=\"%" +
        configItem->key +
        "%\"><br>";
}

//
// buildReportItem
// Configured items are reported to the user in a table (to improve formatting).
// This function builds a row for the table where each row is a separate configuration item.
//
// Parameter: *configItem - A pointer to a configItemData data structure.
//    The structure contains the data needed to build the row.
//
// Returns HTLM string for a table row. 
//

String buildReportItem(configItemData *configItem){
    //<tr><td>{prettyName} <td>%{key}%<br>
    //sprint needs a pre-defined buffer. The issue is with string inputs, it's hard to figure out a good
    //compromse. 
    //So while String concatination is syntactically messier, it seems to be the cleaner design.
    return "<tr><td>" +
      configItem->displayName +
      " <td>%" +
      configItem->key +
      "%<br>";
}

// handleFormResponse
// Check form data submitted to the webserver for a specific field and update the configuration item 
// with the provided data.
//
// 1) check if the form data contains a parameter that matches the key of the provided configuration item
// 2) Check that the parameter contains data.
// 3) If the parameter contains data, update the configuration item with the value.
//
// Parameter: *configItem - A pointer to a configItemData data structure.
// Parameter: *request - A pointer to the HTML request data provided by the webserver
//
// Return  True if the provided configuration item found a matcing parameter, false of not.
// 
bool handleFormResponse(configItemData *configItem, AsyncWebServerRequest *request) {
  Serial.print("handleFormResponse: ");
  if (request->hasParam(configItem->key, true)) {
    Serial.println(configItem->key);
      if (request->getParam(configItem->key, true)->value().length() < configItem->maxLength){
      configItem->value = request->getParam(configItem->key, true)->value();
      return true;
    }
  }
  return false;
}

//
// getItemValue
// Provide a value string for both the report section and the placeholder in the input section.
// Protected items must have a dummy string provided rather than the actual value.
//
// 1) Check the template variable provided by the webserver against the provided configuration item.
// 2) If the item is a match, check if the item is protected.
// 3) If it is not protected, set a reference to the stored value string.
// 4) If it is protected, check if there is a stored value.
// 5) If it is not emply, set a reference to a dummy string. 
// 6) Return true if the reference was updated, false if the refernece isn't 'valid'.
//
// Parameter: templateVar - teh string for the template variabe from the HTML data to be replaced.
// Parameter: configItem - A pointer to a configItemData data structure.
// Parameter: valueString - The refernce to provide the text to use in place of the template variable.
//
// Returns True if valueString has been updated with approriate data. 
//         False if the variable was not claimed.
//
bool getItemValue(String templateVar, configItemData *configItem, String& valueString){
    static const String dummyString = "********";
    Serial.print(F("looking for "));
    Serial.println(templateVar);
    if (templateVar == configItem->key) {
      Serial.print(F("key claimed, value: "));
        //now see if we return the value string or a dummy
        if (!configItem->protect_pw) {
            Serial.println(configItem->value);
            valueString = configItem->value;
            return true;
        } else {
          if (configItem->value.length() > 0){
            Serial.println(F("returning dummy string"));
            valueString = dummyString;
          }
            return true;
        }
    }
    return false;      
}

//
// loadConfigFile
// Attempt to read the provided filename and load the JSON data into jsonConfig
//
bool loadConfigFile(String configFileLoc)
{
  Serial.println(F("Loading configuration"));
  if (configFileLoc.length() == 0) {
    Serial.println("devConfigData.loadConfigData: No config file set");
    return false; 
  }
  File configFile = LittleFS.open(configFileLoc, "r");
  if (!configFile) {
    Serial.println(F("loadConfigData: failed to read file"));
    return false;
  }
  size_t size = configFile.size();
  //is this part even necessary?
  if (size > 4096) { //using 4K min alloc size for littleFS. Actual size should be smaller
    Serial.println(F("Data file size is too large"));
    return false;
  }
  auto error = deserializeJson(jsonConfig, configFile);
  //need to see if I need to copy the data fromt eh file out before closing. If so, I'll need to break out the steps more.
  configFile.close();
  if (error) {
    Serial.println(F("Failed to parse config file"));
    Serial.println(error.f_str());
    return false;
  }
  return true;
}

//
// saveConfigFile
// Attempt to save jsonConfig to the provided filename to flash.
//
bool saveConfigFile(String configFileLoc) {
  Serial.println(F("saveConfigFile"));
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.remove(configFileLoc)) {
    Serial.println(F("File deleted"));
  }

  // Open file for writing
  File file = LittleFS.open(configFileLoc, "w");
  if (!file) {
    Serial.println(F("Failed to open file for writing"));
    return false;
  }

  // Serialize JSON to file
  if (serializeJson(jsonConfig, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  Serial.println(F("Config saved"));
  // Close the file
  file.close();
  return true;
}

//
// saveConfigFile
// Attempt to erase the provided configuration file. Used to reset the device.
//
bool eraseConfig (String configFileLoc) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.print(F("eraseConfig: Deleting config"));
  //TODO:make sure config gets saved or config file is deleted.
  LittleFS.remove(configFileLoc);
  delay(500);
  return true;
}

