/*
Config mode serves and HTML page. 
Please see the ReadMe concerning the choice to go with pure HTML and use a table 
to get some basic formatting rather than opting for HTML + CSS.

For the purposes of this class, the choice dictates how the class returns data 
to update the HTML page with the configuration info.

This class needs to do the following:
Add/update JSON data with a key defined in the class and the data gathered from
the config page.
A callback to add a row to the table of config items
A form processing callback to handle this class's config items

These classes are intended to be instatiated at runtime by a builder function 
that parses a user defined array of data which define the config objects and the
loaded configuration data.

This will enable someone to add config options by updating the list of config
items rather than have to manually create a new instance of this class manually.
*/


class configObject {
private:
    String &configKey; //JSON config key and form input name.
    String configValue;
    //String &configPrettyName; //A more 'user friendly' name to display in the HTML
    String htmlConfigEntry; //build this once when the class is instantiated. Note that templates can be embedded in templates
    int inputMaxLength;
 
public:

    configObject(String &key, String &prettyName, int maxLength) {
        configKey = key;
        //inputMaxLength = maxLength;
        configFieldEncoding = encoding;
        //Desired output (pseudo code as C++'s string handling is nowhere near as good as more modern languages)
        //<tr><td>{prettyName} <td><input type="text" name="{key}" maxlength="{maxLength}" placeholder="%{key}%"><br>
        htmlConfigEntry = String("<tr><td>" + prettyName + 
            " <td><input type=\"text\" name=\"" + key + 
            "maxlength=\"" + String(maxLength) + "\"" +
            "\" placeholder=\"%" + key + "%\"><br>");
    }
    
    configObject(String &key, String &prettyName, int maxLength, JsonDocument &configData) {
        configObject(key, prettyName, maxLength);
        //need to check if the key exists and validate the data.
        if(configData.containsKey(key)) {
            configValue = configData[key];
        }
    }
    
    String getConfigField(){
        return htmlConfigEntry;
    }
    
    String getConfigData(){
        return configValue;
    }
    
    int setConfigData(String data) {
        //encoding and size limits should have been taken care of by the browser. 
        //but this is outside data and should be checked.
        //Need to check with community about the async webserver's ability to validate character sets for form data.
        if (data.lenght() > inputMaxLength) {
            return -1;
        }
        configValue = data;
        reutrn data.lenght();
    }


}