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

#ifndef BACKOFF_H_
#define BACKOFF_H_

#include <Arduino.h>

//
// DEVICE_DUTY_CYCLE_MS
//
// How long the device deep-sleeps between healthy reporting cycles.
// The early entries of the backoff schedule (in backoff.cpp) are
// expressed as multiples of this value, so re-tuning the duty cycle
// for a particular project keeps the early backoff pattern
// proportional to normal cadence ("first miss is one normal cycle of
// patience, second miss doubles it, ..."). The plateau and ceiling
// entries are hard-coded because their semantics ("a human probably
// needs to look at this", "extended outage check-in cadence") do not
// scale with the device's normal reporting cadence.
//
// To override per project, define DEVICE_DUTY_CYCLE_MS before this
// header is included (e.g. via a -D build flag, or in a project-level
// config header that pulls this in).
//
#ifndef DEVICE_DUTY_CYCLE_MS
#define DEVICE_DUTY_CYCLE_MS (60UL * 1000UL)  // 1 minute
#endif

//
// Progressive-backoff API.
//
// The state machine has exactly three exit paths from an awake cycle:
//   1. End-to-end success  : backoffMarkSuccess(); cleanDeepSleep(...)
//   2. End-to-end failure  : backoffMarkFailureAndSleep()
//   3. Healthy nap         : cleanDeepSleep(ms)
//
// "End-to-end" here means whatever the project considers a complete,
// useful cycle. For the canonical environmental-sensor use case that's
// "WiFi connected AND MQTT publish acknowledged"; nothing earlier
// counts, since the whole stack must be working for the device to do
// useful work.
//
// Example use from project code:
//
//   void loopDevMode() {
//     readSensors();
//     if (publishMqtt()) {
//       backoffMarkSuccess();
//       cleanDeepSleep(DEVICE_DUTY_CYCLE_MS);
//     } else {
//       backoffMarkFailureAndSleep();
//     }
//     // Both branches deep-sleep; this function does not return.
//   }
//
// In addition, DevModeWifi() in deviceMode.cpp calls
// backoffMarkFailureAndSleep() automatically on a Wi-Fi connect
// timeout, so a Wi-Fi outage at boot enters backoff without the
// project needing to wire that path itself.
//

//
// Reset the backoff counter to "healthy" (level 0). Call this from
// project code on a confirmed end-to-end success. Does NOT sleep; the
// project owns the normal duty-cycle sleep that follows.
//
void backoffMarkSuccess();

//
// Increment the backoff level (capped at the schedule length) and
// enter the corresponding deep sleep cycle. Saves Wi-Fi state if a
// connection is currently up (so the next wake can fast-resume), and
// calls clearResetCount() so the resulting wake is not interpreted as
// a user-press for the multi-press mode-change signal.
//
// Does NOT return: the device deep-sleeps from inside this call.
//
void backoffMarkFailureAndSleep();

//
// End the awake cycle by deep-sleeping for the supplied duration.
// Saves Wi-Fi state, calls clearResetCount(), then deep-sleeps. Used
// for normal end-of-cycle naps (project passes DEVICE_DUTY_CYCLE_MS
// or its own value); also used internally by
// backoffMarkFailureAndSleep() with the schedule-derived duration.
//
// Does NOT return.
//
void cleanDeepSleep(uint32_t sleepMs);

#endif
