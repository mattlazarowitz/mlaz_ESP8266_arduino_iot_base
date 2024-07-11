#include "configItems.h"

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

int handleFormResponse(configItemData *configItem, JsonDocument &jsonConfig, AsyncWebServerRequest *request) {
  if (request->hasParam(configItem->key, true)) {
    //configSaved = false;
    Serial.println(configItem->key);
    String inputMessage = request->getParam(configItem->key, true)->value();
    //char json_key[configItem->key.length()] = configItem->key.c_str();
    if (inputMessage.length() > 0) {
      //configWiFiSsid = inputMessage;
      //devConfig.setConfigSsid(inputMessage);
      jsonConfig[configItem->key] = inputMessage.c_str();
      return 0;
    }
  }
  return -1;
}

const char* getItemValue(String templateVar, configItemData *configItem, JsonDocument &jsonConfig){
    static char dummyString[] = "********";
    if (templateVar == configItem->key && jsonConfig.containsKey(configItem->key)) {
        //now see if we return the value string or a dummy
        if (!configItem->protect_pw) {
            //return static_cast<String>(jsonConfig[configItem->key]);
            //do type checks later? Just get it working for now.
            return jsonConfig[configItem->key];
        } else {
            //todo: check if there is daya or not and return an indicator
            return dummyString;
        }
    }
    return NULL;      
}