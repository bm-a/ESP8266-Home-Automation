// Triple-reset recovery backed by ESP8266 RTC user memory.
//
// Concept: khoih-prog's ESP_DoubleResetDetector (count boots inside a window).
// Mechanism: ESP8266 Arduino core `ESP.rtcUserMemoryRead/Write` — RTC memory
// survives resets (incl. the RESET button and ESP.restart) but NOT power loss,
// which is exactly the semantics a "press reset N times" detector needs.
// Uses words 64..66, clear of the SDK-reserved low region.
//
// Rule: call resetRecoveryBoot() early in setup(). Before ANY intentional
// ESP.restart(), call resetRecoveryDisarm() so self-reboots are never
// mistaken for button presses.
#pragma once

// Returns the persisted consecutive-boot count (0 on power-up/first boot).
int resetRecoveryCount();
// Persist a count (device mirrors ha::ResetWindow here).
void resetRecoverySave(int count);
// Clear the count: call after the window expires (uptime) or after recovery.
void resetRecoveryClear();
// Mark the next reboot as intentional (not a button press).
void resetRecoveryDisarm();
