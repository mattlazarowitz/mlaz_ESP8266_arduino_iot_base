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
#include <ESPAsyncWebServer.h>  // pulled in transitively by configItems.hpp
#include <include/WiFiState.h>
#include <RTCMemory.h>

#include "configItems.hpp"
#include "rtcInterface.hpp"
#include "backoff.hpp"

//
// Progressive-backoff schedule. Indexed by devRtcData::backoffLevel.
//
// Phase 1 (levels 0-3): doubling, scaled to DEVICE_DUTY_CYCLE_MS.
//   Covers the typical "infrastructure rebooting" recovery window. AP
//   tends to come back first, then router/DHCP, then any application
//   server. Scaling with the duty cycle keeps these proportional to
//   "how patient the device normally is" so that a sensor with a
//   slower duty cycle naturally tolerates longer infrastructure
//   restarts without a separate config knob.
//
// Phase 2 (levels 4-6): plateau at 5 minutes, hard-coded.
//   "Something probably needs human attention but is not an extended
//   outage." The plateau gives the user three patient checks at a
//   human-attention cadence before the device escalates to the
//   long-haul behavior. Hard-coded because "5 minutes is roughly how
//   long it takes a person to notice and act" doesn't scale with how
//   often the device normally reports.
//
// Phase 3 (levels 7-8): ramp to 30 minute ceiling, hard-coded.
//   Extended outage. The device just checks in occasionally to
//   conserve battery while still recovering quickly once the world
//   comes back. Hard-coded for the same reason as the plateau.
//
// Total time to reach ceiling from first failure (with the default
// 1 min duty cycle): ~45 minutes. The schedule never reaches "give up";
// at level 8 the level stops incrementing and the device retries
// every 30 minutes forever, until either a successful cycle resets it
// or the user intervenes via the multi-press reset signal.
//
static const uint32_t backoffScheduleMs[] = {
  1UL * DEVICE_DUTY_CYCLE_MS,    // level 0:  1x duty cycle  (free retry on next normal cycle)
  2UL * DEVICE_DUTY_CYCLE_MS,    // level 1:  2x duty cycle
  4UL * DEVICE_DUTY_CYCLE_MS,    // level 2:  4x duty cycle
  8UL * DEVICE_DUTY_CYCLE_MS,    // level 3:  8x duty cycle  (end of doubling phase)
  5UL * 60UL * 1000UL,           // level 4:    5 min        (plateau)
  5UL * 60UL * 1000UL,           // level 5:    5 min
  5UL * 60UL * 1000UL,           // level 6:    5 min        (end of plateau)
  15UL * 60UL * 1000UL,          // level 7:   15 min        (ramp to ceiling)
  30UL * 60UL * 1000UL           // level 8:   30 min        (ceiling; stays here)
};

static constexpr size_t backoffScheduleLen =
    sizeof(backoffScheduleMs) / sizeof(backoffScheduleMs[0]);

void backoffMarkSuccess() {
  if (!rtcInit) {
    // Without RTC RAM we have no place to track the level; nothing to
    // reset. Healthy operation in this state is degraded but not
    // broken.
    return;
  }
  devRtcData* data = rtcMemIface.getData();
  if (data == nullptr) {
    return;
  }
  if (data->backoffLevel != 0) {
    Serial.print(F("Backoff: success after level "));
    Serial.print(data->backoffLevel);
    Serial.println(F("; resetting to 0"));
    data->backoffLevel = 0;
    rtcMemIface.save();
  }
}

void cleanDeepSleep(uint32_t sleepMs) {
  // If we are currently associated, save Wi-Fi state for fast resume on
  // the next wake. Skip on a never-connected cycle (e.g. backoff
  // failure path before any connect attempt succeeded) since there is
  // no useful state to save and shutdown() may misbehave if called
  // without an established connection.
  if (rtcInit && WiFi.status() == WL_CONNECTED) {
    devRtcData* data = rtcMemIface.getData();
    if (data != nullptr) {
      WiFi.shutdown(data->state);
      rtcMemIface.save();
    }
  }
  // Mark this as a clean shutdown so the wake is not interpreted as a
  // user-press in the multi-press mode-change signal. See the
  // reset-counter contract block in commonInit().
  clearResetCount();

  Serial.print(F("Deep sleep for "));
  Serial.print(sleepMs);
  Serial.println(F(" ms"));
  // Drain the UART before the chip powers down; otherwise the last
  // diagnostic line is often truncated mid-byte on the serial console.
  Serial.flush();

  // ESP.deepSleep() takes microseconds. uint64_t cast guards the
  // multiply against overflow at long sleep durations.
  ESP.deepSleep(static_cast<uint64_t>(sleepMs) * 1000ULL);
  // ESP.deepSleep() does not return.
}

void backoffMarkFailureAndSleep() {
  // Default sleep duration if RTC RAM is unavailable: behave as if
  // every failure is a level-0 event. Better than busy-restarting and
  // retains "device sleeps before retrying" even in the degraded case.
  uint32_t sleepMs = backoffScheduleMs[0];

  if (rtcInit) {
    devRtcData* data = rtcMemIface.getData();
    if (data != nullptr) {
      // Clamp to last index inclusive: at the ceiling we stop
      // incrementing but keep using the same sleep duration on every
      // subsequent failure, so the device keeps checking in at the
      // ceiling cadence forever.
      if (data->backoffLevel < backoffScheduleLen - 1) {
        data->backoffLevel += 1;
        rtcMemIface.save();
      }
      sleepMs = backoffScheduleMs[data->backoffLevel];
      Serial.print(F("Backoff: failure, level now "));
      Serial.print(data->backoffLevel);
      Serial.print(F(" ("));
      Serial.print(sleepMs);
      Serial.println(F(" ms)"));
    }
  } else {
    Serial.println(F("Backoff: RTC RAM unavailable; using level-0 sleep"));
  }

  cleanDeepSleep(sleepMs);
  // cleanDeepSleep() does not return.
}
