// GitHub-release OTA: "press Update, get the latest GitHub release".
//
// Flow: HTTPS GET api.github.com → ha::ghota::parseLatest finds tag + .bin
// asset → HTTPS download (follows github.com → objects.* redirect) →
// flash via the Updater class (same engine as the Arduino OTA core).
// TLS: BearSSL with setInsecure() — see SOURCES.md; acceptable on a trusted
// LAN because the flashed image is only accepted if the Update checksum
// closes cleanly, and releases come from the owner's repo. (ESP8266 Arduino
// core BearSSL docs; GitHub REST "Get the latest release" API.)
#pragma once
#include <string>

#define HA_GH_OWNER "bm-a"
#define HA_GH_REPO "ESP8266-Home-Automation"

// Fetch latest tag + .bin URL. Empty tag = check failed (offline/API error).
// Logs nothing; caller decides. `bodyOut` receives raw JSON when non-null.
bool ghCheckLatest(std::string& tagOut, std::string& binUrlOut,
                   std::string* bodyOut = nullptr);

// Download `binUrl` and flash it. Returns true on verified success.
// Caller MUST reboot afterwards. Progress goes to `logFn` (may be null).
bool ghDownloadAndFlash(const std::string& binUrl,
                        void (*logFn)(const std::string&) = nullptr);
