#include "configItems.h"

JsonDocument jsonConfig;

String buildInputFormItem(configItemData *configItem){
    //<tr><td>{prettyName} <td><input type="text" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
    //"<tr><td>%s <td><input type="text" name="%s" maxlength="%d" placeholder="%%%s%%"><br>"
    //sprint needs a pre-defined buffer. The issue is with string inputs, it's hard to figure out a good
    //compromse. 
    //So while String concatination is syntactically messier, it seems to be the cleaner design.
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

String buildReportItem(configItemData *configItem){
    //<tr><td>{prettyName} <td><input type="text" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
    //"<tr><td>%s <td><input type="text" name="%s" maxlength="%d" placeholder="%%%s%%"><br>"
    //sprint needs a pre-defined buffer. The issue is with string inputs, it's hard to figure out a good
    //compromse. 
    //So while String concatination is syntactically messier, it seems to be the cleaner design.
    if (!configItem->protect_pw) {
        return "<tr><td>" +
          configItem->displayName +
          " <td>%" +
          configItem->key +
          "%<br>";
    }
    //todo: make this prettier
        return "<tr><td>" +
          configItem->displayName +
          " set<td>%" +
          configItem->key +
          "%<br>";
}

int handleFormResponse(configItemData *configItem, AsyncWebServerRequest *request) {
  Serial.print("handleFormResponse: ");
  if (request->hasParam(configItem->key, true)) {
    //configSaved = false;
    Serial.println(configItem->key);
    //String inputMessage = request->getParam(configItem->key, true)->value();
    //char json_key[configItem->key.length()] = configItem->key.c_str();
    //if (inputMessage.length() > 0) {
      if (request->getParam(configItem->key, true)->value().length() < configItem->maxLength){
      configItem->value = request->getParam(configItem->key, true)->value();
      return 0;
    }
  }
  return -1;
}

//
// Design of this has become a bit subtle as parameters have been fleshed out.
// Value strings need to be allowed to be empt
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
            //todo: check if there is data or not and return an indicator
            //return dummyString;
            valueString = dummyString;
          }
            return true;
        }
    }
    return false;      
}

bool configToJson (configItemData *configItem, JsonDocument jsonConfig) {
  //load stuff from config into config item
  return true;
}






bool loadConfigFile(String configFileLoc)
{
  Serial.println(F("Loading configuration"));
  if (configFileLoc.length() == 0) {
    Serial.println("devConfigData.loadConfigData: No config file set");
    return false; 
  }
  File configFile = LittleFS.open(configFileLoc, "r");
  if (!configFile) {
    Serial.println(F("devConfigData.loadConfigData: failed to read file"));
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

bool eraseConfig (String configFileLoc) {
  //no data, we just go ahead and delete the config file
  //TODO: Move to config object
  Serial.print(F("eraseConfig: Deleting config"));
  //TODO:make sure config gets saved or config file is deleted.
  LittleFS.remove(configFileLoc);
  delay(500);
  return true;
}

