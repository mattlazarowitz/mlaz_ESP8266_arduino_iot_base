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
#include <ESP8266TimerInterrupt.h>
#include <include/WiFiState.h>
#include <RTCMemory.h>

#include "configItems.hpp"
#include "HtmlRequests.hpp"
#include "rtcInterface.hpp"

//
// Forward declarations for every per-mode setup / loop function so the
// ModeOps dispatch table further down can reference them. setupDevMode
// and loopDevMode live in deviceMode.cpp; the rest are defined in this
// translation unit.
//
//TODO: see if this can go into a header file when I do the header file cleanup.
void setupDevMode();
void loopDevMode();
void setupReconfigMode();
void setupApConfigMode();
static void setupResetMode();
static void loopNoop();

//TODO: Find a better place for this
#define AP_IP_ADDR 192,168,30,1
//Sketch specific data types


//
// devOpMode: which of the device's normal runtime modes setup() should
// dispatch into. This intentionally describes only *successful* init
// outcomes; hard failures during commonInit() are represented separately
// via commonInitStatus so the two concerns aren't tangled in one enum.
//
enum devOpMode {
  staDevice,   // regular mode. Device is in station mode and runs the normal device function loop.
  staConfig,   // station mode on the configured WiFi network, but boots into the configuration web server instead of the device function loop.
  apConfig,    // AP mode config: device comes up as its own AP so a client can connect directly and reach the config page.
  resetConfig  // factory-reset request: erase the stored config and restart.
};

//
// commonInitStatus: result of commonInit(). On `ok`, BootMode is set and
// setup() can dispatch normally. Anything else is a hard pre-dispatch
// failure that setup() should route to handleInitError() rather than the
// devOpMode switch. Kept as a separate enum from devOpMode so future
// failure modes (RTC corruption, OTA-only recovery states, etc.) can be
// added without overloading the dispatch enum.
//
enum class commonInitStatus {
  ok,
  fsMountFailed
};


//globals
AsyncWebServer server(80);
ESP8266Timer ITimer;
RTCMemory<devRtcData> rtcMemIface;
// BootMode is overwritten by commonInit() before the dispatch table is
// consulted; the apConfig default is a defensive fallback so that any
// hypothetical path which read it without commonInit having run first
// would land in the safest state (unconfigured-device, AP-mode config
// page) rather than attempting to bring up real device functionality
// against unloaded config.
devOpMode BootMode = apConfig;
// rtcInit gates both clearResetCount() and the TimerHandler ISR; the
// false default ensures both are no-ops until rtcMemIface.begin() has
// successfully run.
bool rtcInit = false;

//
// Simple debug function to convert the boot mode into a string. Takes
// the mode by value (parameter intentionally not named BootMode so it
// does not shadow the global of the same name).
//
String bootModeToStr(devOpMode mode) {
  switch (mode) {
    case staDevice:
      return "staDevice";
    case staConfig:
      return "staConfig";
    case apConfig:
      return "apConfig";
    case resetConfig:
      return "resetConfig";
    default:
      break;
  }
  return "invalid boot mode";
}

//
// Timer ISR. 
// Used for boot mode change command.
// Resets within a predetermined time window are counted and used to determine a user-commanded boot mode change. 
// But if the count is reset at the end of Setup(), it's very hard for a human to use this functionality.
// So this timer ISR creates a 750ms window instead. 
// In battery powered device mode, this could become an issue if the device resets itself too quickly.
// In that case, it may be better to add the reset functionality right before the reset or deep sleep command.
// The process of reading a sensor, restoring the WiFi connection, and reporting the data should hopefully take 
// long enough to allow for reliable reset detection.
//
void IRAM_ATTR TimerHandler()
{
  // The timer is only ever armed (in commonInit) after rtcMemIface has
  // been successfully initialized, but guard the access defensively so
  // a future reordering or a partial-init path cannot turn this ISR
  // into the source of garbage RTC writes.
  if (rtcInit) {
    devRtcData* myRtcData = rtcMemIface.getData();
    myRtcData->unhandledResetCount = 0;
    rtcMemIface.save();
  }
  //This may be bad, but it seems to work OK for now.
  timer1_disable();
}

//
// Setup() sub-function
// This is the version of the Setup() function that needs to be called when the 
// ESP8266 is operating in apConfig mode. 
// The device comes up with WiFi in AP mode and runs a webserver to serve the config pages.
// 
void setupApConfigMode()
{
  Serial.println("setupApConfigMode");
  String configEspHostname;
  if (jsonConfig.containsKey("hostname")) {
    configEspHostname = String("config:") + String(jsonConfig["hostname"]);
  } else {
    configEspHostname = String("config:") + WiFi.hostname().c_str();
  }
  WiFi.softAPConfig(IPAddress(AP_IP_ADDR), IPAddress(0,0,0,0), IPAddress(255,255,255,0));
  Serial.print(F("AP hostname: "));
  Serial.println(configEspHostname);
  WiFi.softAP(configEspHostname);
  // At full power, the device can experience brownouts. 
  // When debugging, these look like random disconnects on USB.
  // Reduce power to make this easier on a PC's USB port
  WiFi.setOutputPower(15);

  // setup HTTP server and the HTML requests
  registerHtmlInterfaces();
  server.begin();
}

//
// Setup() sub-function
// This is the version of the Setup() function that needs to be called when the 
// ESP8266 is operating in staConfig mode. 
// The device connects to the configured WiFi network but runs the webserver to 
// serve the configuration pages rather than operate an IoT device.
// 
void setupReconfigMode()
{
  Serial.println("setupReconfigMode");
  WiFi.hostname(static_cast<String>(jsonConfig["hostname"]).c_str());
  Serial.print(F("Connecting to "));
  Serial.println(static_cast<String>(jsonConfig["ssid"]));

  WiFi.mode(WIFI_STA);
  WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["WiFiPw"]));

  const unsigned long connectTimeoutMs = 30000;
  const unsigned long connectStartMs = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStartMs >= connectTimeoutMs) {
      Serial.println();
      Serial.println(F("staConfig WiFi connect timed out, restarting"));
      // Clean software restart: see the reset-counter contract in commonInit().
      clearResetCount();
      ESP.restart();
      delay(1000);
    }
    delay(500);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
  Serial.println(WiFi.localIP());

  registerHtmlInterfaces();
  server.begin();
}


//
// There are some common steps needed by all boot modes. Those get performed here.
//
// On success returns commonInitStatus::ok and sets the global BootMode to
// the appropriate devOpMode for setup() to dispatch into. On a hard
// failure that prevents normal dispatch, returns the corresponding
// failure status; in that case BootMode is undefined and the caller must
// route to handleInitError() rather than the devOpMode switch.
//
commonInitStatus commonInit(){
  devRtcData* myRtcData = nullptr;

  // Hardware Timer1 is the only interrupt-capable timer available for
  // user code on the ESP8266 (Timer0 is reserved by the WiFi stack /
  // Arduino core). attachInterruptInterval() returns false on failure
  // (e.g. interval out of range, already-attached). If it fails the
  // ISR never fires, which means the 750 ms reset-counter clear in
  // TimerHandler() never runs, which means the next reset gets counted
  // as press N+1 of a multi-press. That can drift a perfectly healthy
  // device toward factory-reset over enough resets, so do not let this
  // failure mode be silent. Not treated as fatal: the rest of the
  // device functions normally; only multi-press mode-change is
  // unavailable until the next boot.
  //
  // The 750 ms interval was hand-tuned for human reset-press cadence,
  // and the attach intentionally happens at the very top of
  // commonInit() so the multi-press window is anchored to boot start
  // rather than to the (variable) duration of FS mount and RTC init.
  // Moving the attach later would lengthen the window in a way that
  // makes routine post-boot resets more likely to be misread as press
  // 2 of a sequence. The ISR is additionally guarded by the rtcInit
  // flag to handle the case where the FS-mount or RTC-init steps below
  // fail before the ISR fires.
  if (!ITimer.attachInterruptInterval(750000, TimerHandler)) {
    Serial.println(F("WARN: Timer1 attach failed; multi-press reset "
                     "detection is disabled this boot."));
  }
  Serial.println(F("Mount LittleFS"));
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed"));
    return commonInitStatus::fsMountFailed;
  }

  switch (loadConfigFile(CONFIG_FILE)) {
    case loadConfigResult::loaded:
      Serial.println(F("config loaded"));
      BootMode = staDevice;
      break;
    case loadConfigResult::corrupt:
      // The file existed but could not be parsed. Erase it so we are not
      // stuck repeatedly tripping over the same bad blob, then drop to
      // unconfigured-device behavior so the user can re-enter settings.
      // lastConfigLoadResult remains `corrupt` past this point so the
      // config page can show a one-shot recovery banner; HandleSaveRequest
      // resets it once the user successfully writes a new config.
      Serial.println(F("Config file was corrupt; erasing and entering AP config mode"));
      eraseConfig(CONFIG_FILE);
      BootMode = apConfig;
      break;
    case loadConfigResult::missing:
    default:
      // No config yet: fresh device or post-reset. FS is fine, just bring
      // up AP-mode config so the user can supply settings.
      BootMode = apConfig;
      break;
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
    // We can either reset or try the begin again. 
    // Go ahead and try to begin again. IF that fails, I think the best course of action is an
    // error 'halt' and blink the onboard LED.
    rtcInit = rtcMemIface.begin();
    if (rtcInit){
      myRtcData = rtcMemIface.getData();
    }
  } else {
    Serial.println(F("reading RTC data"));
    myRtcData = rtcMemIface.getData();
  }
  if (myRtcData != nullptr) {
    //
    // Reset-counter contract:
    //
    // The counter is incremented unconditionally on every boot. The 750 ms
    // timer ISR (TimerHandler) clears it once setup has run long enough
    // that we can be confident this was not a quick multi-press from the
    // user. That gives a "did the user mash the reset button" signal
    // without needing an extra GPIO.
    //
    // On ESP8266 the deep-sleep wake pulse is wired through GPIO16 to the
    // reset pin, so a wake from sleep is electrically the same event as a
    // button press. SDK-level reset-cause classification is therefore not
    // a load-bearing distinction here, and we deliberately do not key off
    // it. Instead, any code path that intentionally short-circuits the
    // 750 ms window (a software-initiated ESP.restart, a planned deep
    // sleep, etc.) is responsible for calling clearResetCount() before
    // triggering the reset / sleep so its own wakeup is not counted as a
    // user press.
    //
    Serial.print(F("reset count: "));
    Serial.println(myRtcData->unhandledResetCount);
    myRtcData->unhandledResetCount += 1;
    rtcMemIface.save();
    //now see if we hit any of the manual mode override thresholds
    switch (myRtcData->unhandledResetCount) {
      case 0:
      case 1:
        Serial.println(F("no override"));
        Serial.print(F("Boot mode: "));
        Serial.println(bootModeToStr(BootMode));
        break;
      case 2:
        Serial.println(F("reconfig on configed network"));
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
      // Use 4 or more concurrent resets as the signal to wipe
      // the stored configuration.
      // This handles any case where a reset happens while the config is being erased.
      default:
        Serial.println(F("\"factory reset\""));
        BootMode = resetConfig;
        blinkLed(5);
        break;
    }
  }
  return commonInitStatus::ok;
}
void blinkLed(int blinks) {
  for (int i = 0; i < blinks;i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  }
}

//
// Terminal error path used when commonInit() cannot bring the device far
// enough along to dispatch into one of the usable boot modes. The device
// is, from the user's perspective, bricked: it cannot serve config pages,
// it cannot persist new config, and without config it cannot reach the
// configured network. There is no in-band recovery from this state in
// the current firmware.
//
// FUTURE WORK / DESIGN NOTE:
// This is a deliberately partial design, parked here rather than papered
// over so the gap is visible to a future maintainer (likely future me).
// The intended recovery story is OTA: an OTA pathway that can repopulate
// the LittleFS data partition (HTML pages, and optionally a config file)
// without requiring a USB reflash. Once that exists, this handler should
// attempt OTA recovery before settling into the halt loop. Alternatives
// considered and rejected for this commit: bundling a minimal AP-mode
// config page in PROGMEM and calling LittleFS.format() to recover (works
// but is a separate feature-sized change), and silently formatting the
// FS (destroys user data with no recovery for the HTML, so it just moves
// the brick one step later).
//
// Until that lands, the contract is: log a clear, actionable diagnostic
// to the serial console and run a distinct LED pattern indefinitely so a
// user holding the device knows the failure mode is "needs reflash" and
// not "still booting" or "WiFi confused".
//
void handleInitError(commonInitStatus status) {
  Serial.println();
  switch (status) {
    case commonInitStatus::fsMountFailed:
      Serial.println(F("FATAL: LittleFS mount failed."));
      Serial.println(F("Most likely cause: the LittleFS data partition was"));
      Serial.println(F("never uploaded, or has been corrupted. The device"));
      Serial.println(F("cannot serve config pages or persist settings in"));
      Serial.println(F("this state. Reflash including the data partition."));
      break;
    case commonInitStatus::ok:
      // Should not happen; ok is not an error.
      Serial.println(F("handleInitError called with ok status; ignoring."));
      return;
    default:
      Serial.print(F("FATAL: unknown commonInit status "));
      Serial.println(static_cast<int>(status));
      break;
  }
  // Distinct from the 5-blink "factory reset" indicator emitted in
  // commonInit(): three quick blinks, long pause, repeat forever.
  for (;;) {
    blinkLed(3);
    delay(1500);
  }
}

//
// Mark a software-initiated reset / deep sleep as "clean" so the resulting
// wakeup is not counted toward the multi-press mode-change signal in
// commonInit(). See the reset-counter contract block in commonInit() for
// background.
//
void clearResetCount() {
  if (!rtcInit) {
    return;
  }
  devRtcData* myRtcData = rtcMemIface.getData();
  if (myRtcData != nullptr && myRtcData->unhandledResetCount != 0) {
    myRtcData->unhandledResetCount = 0;
    rtcMemIface.save();
  }
}
//
// Setup() sub-function for resetConfig mode. Erases the persisted config
// file, marks the resulting reset as "clean" so the next boot is not
// counted as another press in a multi-press sequence, and triggers the
// software reset. Does not return on success.
//
static void setupResetMode() {
  Serial.println(F("setupResetMode: erasing config and restarting"));
  eraseConfig(CONFIG_FILE);
  // Clean restart: see the reset-counter contract in commonInit().
  clearResetCount();
  delay(1000);
  ESP.restart();
  delay(1000);
}

//
// Loop function used by modes whose runtime work is fully driven by
// async callbacks (the AsyncWebServer's own request handlers) or whose
// setup never returned (resetConfig). Having an explicit no-op lets the
// dispatch table be uniform: every devOpMode has both a setup and a
// loop entry, and loop() never has to know which modes "do nothing".
//
static void loopNoop() {}

//
// Per-mode dispatch table. Indexed by devOpMode value. Order MUST match
// devOpMode declaration order; the static_assert below catches a length
// mismatch and the row comments make a re-order obvious in review.
//
// Adding a new mode is now a three-step change with no edits to setup()
// or loop():
//   1. Add the new value to devOpMode (in declaration order).
//   2. Add a row here in the same position, pointing at the mode's
//      setup and (optionally) loop functions.
//   3. Implement those functions.
//
struct ModeOps {
  void (*setupFn)();
  void (*loopFn)();
};

static const ModeOps modeOps[] = {
  /* staDevice   */ { setupDevMode,      loopDevMode },
  /* staConfig   */ { setupReconfigMode, loopNoop    },
  /* apConfig    */ { setupApConfigMode, loopNoop    },
  /* resetConfig */ { setupResetMode,    loopNoop    }
};

static_assert(sizeof(modeOps) / sizeof(modeOps[0]) == resetConfig + 1,
              "modeOps must have exactly one entry per devOpMode value, "
              "in the same order as the devOpMode enum.");

void setup() {
  Serial.begin(115200);
  // Brief settle delay after Serial.begin(). Common Arduino-platform
  // idiom: gives the UART hardware a moment to be fully ready before
  // we start streaming diagnostics, avoiding the first few characters
  // being lost or garbled across reset/power-on. ESP8266's hardware
  // UART does not need this for correctness, but keeping it improves
  // reliability of the boot log on slow USB-serial adapters.
  delay(20);
  Serial.println(F("Start setup"));
  pinMode(LED_BUILTIN, OUTPUT);

  const commonInitStatus initStatus = commonInit();
  if (initStatus != commonInitStatus::ok) {
    // Hard pre-dispatch failure. handleInitError() does not return.
    handleInitError(initStatus);
  }

  modeOps[BootMode].setupFn();

  Serial.println("Setup done");
}

void loop() {
  modeOps[BootMode].loopFn();
}
