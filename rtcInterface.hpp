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

#ifndef RTC_INTERFACE_H_
#define RTC_INTERFACE_H_

//
// Data persisted in RTC RAM. Survives deep sleep and software resets;
// lost on power loss (the desired "fresh start" semantic on a power
// cycle).
//
//   unhandledResetCount  Used by the multi-press mode-change detection.
//                        See the reset-counter contract block in
//                        commonInit() for the full design.
//
//   state                Saved Wi-Fi state for fast resume across deep
//                        sleep cycles via WiFi.resumeFromShutdown().
//
//   backoffLevel         Index into the progressive-backoff schedule in
//                        backoff.cpp. 0 = healthy. Bumped (capped at
//                        the schedule length) on each end-to-end
//                        failure, reset to 0 on each end-to-end
//                        success. See backoff.hpp for the API.
//
typedef struct {
  unsigned int unhandledResetCount;
  WiFiState state;
  uint8_t backoffLevel;
} devRtcData;

//please ensure these are in your .ino file.
extern RTCMemory<devRtcData> rtcMemIface;
extern bool rtcInit;

//
// Zero the user-press reset counter in RTC RAM. Call this immediately
// before any software-initiated reset (ESP.restart) or deep sleep so the
// resulting wakeup is treated as a clean boot rather than the next press
// in a multi-press sequence. See commonInit() in the .ino for the full
// contract.
//
void clearResetCount();

#endif