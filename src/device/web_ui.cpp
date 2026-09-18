#include "web_ui.h"
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <Uri.h>
#include "../config.h"
#include "wifi_portal.h"

static ESP8266WebServer s_server(80);
static AppContext* s_web = nullptr;
static bool s_uploadAllowed = false;

// Roles as plain ints: Arduino IDE hoists prototypes above enum definitions,
// so a custom enum type in signatures breaks the single-file sketch build.
static const int R_NONE = 0;
static const int R_USER = 1;
static const int R_ADMIN = 2;

// --- tiny base64 decoder (for "Basic <b64>") ---
static int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static std::string b64decode(const String& in) {
  std::string out;
  int val = 0, bits = 0;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '=') break;
    int v = b64val(c);
    if (v < 0) continue;
    val = (val << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += (char)((val >> bits) & 0xFF);
    }
  }
  return out;
}

static int currentRole() {
  String h = s_server.header("Authorization");
  if (!h.startsWith("Basic ")) return R_NONE;
  std::string cred = b64decode(h.substring(6));
  size_t pos = cred.find(':');
  if (pos == std::string::npos) return R_NONE;
  std::string user = cred.substr(0, pos);
  std::string pass = cred.substr(pos + 1);
  if (user == "admin" && s_web->auth->verifyAdmin(pass)) return R_ADMIN;
  if (user == "user" && s_web->auth->verifyUser(pass)) return R_USER;
  return R_NONE;
}

static bool needAuth(int minimum) {
  int r = currentRole();
  if (r >= minimum && r != R_NONE) return false;
  s_server.requestAuthentication();
  return true;
}

// First-boot gate: force admin password setup before anything else.
static bool setupGate() {
  if (!s_web->auth->adminMustChange()) return false;
  String uri = s_server.uri();
  if (uri == "/setup" || uri == "/api/setup" || uri == "/api/state")
    return false;
  s_server.sendHeader("Location", "/setup");
  s_server.send(302, "text/plain", "Setup required");
  return true;
}

static const char* kCss =
    "body{background:#000;color:#fff;font-family:'Courier New',monospace;"
    "margin:0}main{max-width:720px;margin:0 auto;padding:20px}"
    "h1{font-size:22px;border-bottom:2px solid #2ecc40;padding-bottom:6px}"
    "button{background:#2ecc40;color:#000;border:0;padding:10px 22px;font-size:16px;"
    "cursor:pointer;margin:4px;font-family:inherit}"
    "button.off{background:#555;color:#fff}"
    "input{background:#111;color:#fff;border:1px solid #2ecc40;padding:8px;margin:4px;"
    "font-family:inherit}label{display:block;margin-top:8px}"
    ".card{border:1px solid #2ecc40;padding:12px;margin:12px 0}"
    ".log{border:1px solid #fff;padding:10px;max-height:300px;overflow-y:auto;"
    "font-size:13px}.row{display:flex;align-items:center;gap:8px;margin:6px 0}"
    "nav a{color:#2ecc40;margin-right:14px}";

static void page(const String& title, const String& body) {
  String html = "<html><head><meta name='viewport' content='width=device-width'>"
                "<title>" +
                title + "</title><style>" + kCss +
                "</style></head><body><main><nav><a href='/'>Home</a>"
                "<a href='/logs'>Logs</a><a href='/settings'>Settings</a>"
                "<a href='/update'>OTA</a></nav><h1>" +
                title + "</h1>" + body + "</main></body></html>";
  s_server.send(200, "text/html", html);
}

static void handleRoot() {
  if (setupGate()) return;
  if (needAuth(R_USER)) return;
  String body =
      "<div class='card'><div id='st'>loading…</div></div>"
      "<div class='card' id='relays'></div>"
      "<script>"
      "async function st(){const r=await fetch('/api/state');const j=await r.json();"
      "document.getElementById('st').innerHTML='Time '+j.time+' | IP '+j.ip+' | RSSI '+j.rssi+' dBm';"
      "let h='';j.relays.forEach((s,i)=>{h+='<div class=row><b>Relay '+(i+1)+'</b> '+(s?'ON':'OFF')+"
      "' <button '+(s?'':'class=off')+' onclick=\"setR('+i+','+(s?'0':'1')+')\">'+(s?'Turn OFF':'Turn ON')+'</button></div>'});"
      "document.getElementById('relays').innerHTML=h;}"
      "async function setR(i,on){await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ch='+i+'&on='+on});st();}"
      "st();setInterval(st,2000);</script>";
  page("ESP Home", body);
}

static void handleSetup() {
  if (!s_web->auth->adminMustChange()) {
    s_server.sendHeader("Location", "/");
    s_server.send(302, "text/plain", "Already set up");
    return;
  }
  String body =
      "<div class='card'><p>First boot: set the admin password to unlock the "
      "device. (Default credentials are intentionally absent.)</p>"
      "<label>Admin password (min 8 chars)<input type='password' id='pw'></label>"
      "<button onclick=\"f()\">Save &amp; unlock</button><p id='m'></p></div>"
      "<script>async function f(){const pw=document.getElementById('pw').value;"
      "const r=await fetch('/api/setup',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'password='+encodeURIComponent(pw)});"
      "document.getElementById('m').innerText=await r.text();if(r.ok)location='/';}</script>";
  page("First-time setup", body);
}

static void handleApiSetup() {
  if (!s_web->auth->adminMustChange()) {
    s_server.send(403, "text/plain", "Already set up");
    return;
  }
  String pw = s_server.arg("password");
  if (pw.length() < 8) {
    s_server.send(400, "text/plain", "Password must be >= 8 characters");
    return;
  }
  s_web->auth->setAdminPassword(pw.c_str());
  if (s_web->otaPassword->empty()) *s_web->otaPassword = pw.c_str();
  s_web->saveAll();
  s_web->addLog("Admin password set (first boot)");
  s_server.send(200, "text/plain", "OK - now log in as admin");
}

static String stateJson() {
  String j = "{";
  j += "\"time\":\"" + String(s_web->fmtTime(s_web->epochNow()).c_str()) + "\",";
  j += "\"ip\":\"" + String(wifiIp().c_str()) + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ntp\":" + String(*s_web->ntpOk ? "true" : "false") + ",";
  j += "\"uptime\":" + String(millis() / 1000) + ",";
  j += "\"relays\":[";
  for (int i = 0; i < s_web->channels; i++) {
    if (i) j += ",";
    j += s_web->relays->get(i) ? "true" : "false";
  }
  j += "]}";
  return j;
}

static void handleApiState() {
  if (setupGate()) return;
  if (needAuth(R_USER)) return;
  s_server.send(200, "application/json", stateJson());
}

static void applyRelay(int ch, bool on, const char* src) {
  if (!s_web->relays->set(ch, on)) {
    s_server.send(400, "text/plain", "Invalid channel");
    return;
  }
  digitalWrite(s_web->relayPins[ch], s_web->relays->levelForChannel(ch));
  char msg[64];
  snprintf(msg, sizeof(msg), "Relay %d %s (%s)", ch + 1, on ? "ON" : "OFF", src);
  s_web->addLog(msg);
  s_server.send(200, "application/json",
                "{\"ch\":" + String(ch) + ",\"on\":" + (on ? "true" : "false") + "}");
}

static void handleApiRelay() {
  if (setupGate()) return;
  if (needAuth(R_USER)) return;
  int ch = s_server.arg("ch").toInt();
  if (s_server.hasArg("on"))
    applyRelay(ch, s_server.arg("on") == "1", "web");
  else
    applyRelay(ch, !s_web->relays->get(ch), "web");  // toggle
}

static void handleRelayToggleLegacy(int i) {
  // POST /relay/<i> — v3-compatible shape, now actually routed.
  if (setupGate()) return;
  if (needAuth(R_USER)) return;
  if (i < 0 || i >= s_web->channels) {
    s_server.send(400, "text/plain", "Invalid relay index");
    return;
  }
  applyRelay(i, !s_web->relays->get(i), "web");
}

static void handleLogs() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String body = "<div class='log' id='l'>loading…</div><script>"
                "fetch('/api/logs').then(r=>r.text()).then(t=>{"
                "document.getElementById('l').innerText=t;});</script>";
  page("Event log", body);
}

static void handleApiLogs() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  auto lines = s_web->log->rendered(
      [](uint32_t e) { return s_web->fmtTime(e); });
  String out;
  for (auto& l : lines) {
    out += String(l.c_str());
    out += "\n";
  }
  s_server.send(200, "text/plain", out);
}

static void handleSettings() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String body =
      "<div class='card'><h3>Admin password</h3>"
      "<input type='password' id='ap' placeholder='new admin password'>"
      "<button onclick=\"p('/api/adminpw',{password:document.getElementById('ap').value})\">Change</button></div>"
      "<div class='card'><h3>User account (limited: relays only)</h3>"
      "<label><input type='checkbox' id='ue'> enabled</label>"
      "<input type='password' id='up' placeholder='user password'>"
      "<button onclick=\"p('/api/userset',{enabled:document.getElementById('ue').checked?1:0,password:document.getElementById('up').value})\">Save</button></div>"
      "<div class='card'><h3>OTA password (Arduino IDE + HTTP upload)</h3>"
      "<input type='password' id='op' placeholder='ota password'>"
      "<button onclick=\"p('/api/otapw',{password:document.getElementById('op').value})\">Change</button></div>"
      "<div class='card'><h3>Device</h3>"
      "<button onclick=\"p('/api/portal',{})\">Open WiFi portal</button> "
      "<button onclick=\"if(confirm('Reboot?'))p('/api/restart',{})\">Reboot</button></div>"
      "<p id='m'></p>"
      "<script>async function p(u,b){const f=new URLSearchParams(b);"
      "const r=await fetch(u,{method:'POST',body:f});"
      "document.getElementById('m').innerText=await r.text();}</script>";
  page("Settings", body);
}

static void handleAdminPw() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String pw = s_server.arg("password");
  if (pw.length() < 8) {
    s_server.send(400, "text/plain", "Min 8 characters");
    return;
  }
  s_web->auth->setAdminPassword(pw.c_str());
  s_web->saveAll();
  s_web->addLog("Admin password changed");
  s_server.send(200, "text/plain", "Admin password changed");
}

static void handleUserSet() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  bool en = s_server.arg("enabled") == "1";
  String pw = s_server.arg("password");
  if (en && pw.length() < 4) {
    s_server.send(400, "text/plain", "User password min 4 characters");
    return;
  }
  s_web->auth->setUserEnabled(en);
  if (pw.length()) s_web->auth->setUserPassword(pw.c_str());
  s_web->saveAll();
  s_web->addLog(en ? "User account enabled" : "User account disabled");
  s_server.send(200, "text/plain", "User settings saved");
}

static void handleOtaPw() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String pw = s_server.arg("password");
  if (pw.length() < 8) {
    s_server.send(400, "text/plain", "Min 8 characters");
    return;
  }
  *s_web->otaPassword = pw.c_str();
  s_web->saveAll();
  s_web->addLog("OTA password changed (applies after reboot)");
  s_server.send(200, "text/plain", "OTA password saved - reboot to apply");
}

static void handlePortal() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  s_server.send(200, "text/plain", "Opening portal…");
  delay(500);
  s_web->requestPortal();
}

static void handleRestart() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  s_server.send(200, "text/plain", "Rebooting…");
  delay(500);
  s_web->requestReboot();
}

static void handleUpdatePage() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String body =
      "<div class='card'><p>Upload a compiled .bin (same build). "
      "Device reboots automatically.</p>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'><button type='submit'>Upload</button>"
      "</form></div>";
  page("Firmware update", body);
}

static void handleUpdateUpload() {
  HTTPUpload& up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    s_uploadAllowed = (currentRole() == R_ADMIN);
    if (s_uploadAllowed) {
      uint32_t maxSketch = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      Update.begin(maxSketch);
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_uploadAllowed) Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_uploadAllowed && Update.end(true))
      s_web->addLog("OTA update applied via HTTP");
    else if (s_uploadAllowed)
      s_web->addLog("OTA update FAILED");
  }
}

static void handleUpdateDone() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  if (!s_uploadAllowed || Update.hasError()) {
    s_server.send(403, "text/plain", "Update failed or not authorized");
    return;
  }
  s_server.send(200, "text/plain", "Update OK - rebooting…");
  delay(500);
  ESP.restart();
}

static void handleNotFound() {
  // Legacy relay path with manual parse (core 3.1.x has no UriBraces).
  String uri = s_server.uri();
  if (s_server.method() == HTTP_POST && uri.startsWith("/relay/")) {
    handleRelayToggleLegacy(uri.substring(7).toInt());
    return;
  }
  s_server.send(404, "text/plain", "Not found");
}

void webBegin(AppContext* ctx) {
  s_web = ctx;
  s_server.collectHeaders("Authorization");
  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/setup", HTTP_GET, handleSetup);
  s_server.on("/api/setup", HTTP_POST, handleApiSetup);
  s_server.on("/api/state", HTTP_GET, handleApiState);
  s_server.on("/api/relay", HTTP_POST, handleApiRelay);
  s_server.on("/logs", HTTP_GET, handleLogs);
  s_server.on("/api/logs", HTTP_GET, handleApiLogs);
  s_server.on("/settings", HTTP_GET, handleSettings);
  s_server.on("/api/adminpw", HTTP_POST, handleAdminPw);
  s_server.on("/api/userset", HTTP_POST, handleUserSet);
  s_server.on("/api/otapw", HTTP_POST, handleOtaPw);
  s_server.on("/api/portal", HTTP_POST, handlePortal);
  s_server.on("/api/restart", HTTP_POST, handleRestart);
  s_server.on("/update", HTTP_GET, handleUpdatePage);
  s_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_server.onNotFound(handleNotFound);
  s_server.begin();
}

void webLoop() { s_server.handleClient(); }
