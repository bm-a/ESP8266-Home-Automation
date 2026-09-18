#include "web_ui.h"
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <Uri.h>
#include "../config.h"
#include "../logic/ghota.h"
#include "../logic/guard.h"
#include "../logic/session.h"
#include "gh_update.h"
#include "wifi_portal.h"

static ESP8266WebServer s_server(80);
static AppContext* s_web = nullptr;
static bool s_uploadAllowed = false;
static ha::SessionStore s_sessions;
static ha::AttemptTracker s_guard;
static uint32_t s_lastSweep = 0;

// Roles as plain ints: Arduino IDE hoists prototypes above enum definitions,
// so a custom enum type in signatures breaks the single-file sketch build.
static const int R_NONE = 0;
static const int R_USER = 1;
static const int R_ADMIN = 2;

static const char* kCookieName = "HA_SESSION=";

// Every response carries these (OWASP secure-headers cheat sheet, embedded
// subset: no framing, no MIME sniffing, no plugins, tight referrer).
static void sendSecure(int code, const String& type, const String& body) {
  s_server.sendHeader("X-Content-Type-Options", "nosniff");
  s_server.sendHeader("X-Frame-Options", "DENY");
  s_server.sendHeader("Referrer-Policy", "no-referrer");
  s_server.sendHeader("Content-Security-Policy",
                      "default-src 'self'; script-src 'unsafe-inline'; "
                      "object-src 'none'; base-uri 'self'; "
                      "frame-ancestors 'none'");
  sendSecure(code, type, body);
}

static std::string sessionToken() {
  String c = s_server.header("Cookie");
  int i = c.indexOf(kCookieName);
  if (i < 0) return std::string();
  int j = c.indexOf(';', (size_t)(i + 11));
  String t = (j < 0) ? c.substring(i + 11) : c.substring(i + 11, j);
  t.trim();
  return std::string(t.c_str());
}

static std::string clientIp() {
  return std::string(s_server.client().remoteIP().toString().c_str());
}

static int currentRole() {
  int role = s_sessions.validate(sessionToken(), millis());
  if (role == ha::kRoleAdmin) return R_ADMIN;
  if (role == ha::kRoleUser) return R_USER;
  return R_NONE;
}

// CSRF: state-changing requests must come from our own pages (Origin match)
// or from non-browser clients (no Origin header, e.g. curl). Cookies are
// SameSite=Strict as the second layer.
static bool originOk() {
  String o = s_server.header("Origin");
  if (o.length() == 0) return true;
  String host = s_server.hostHeader();
  int s = o.indexOf("://");
  String oh = (s >= 0) ? o.substring(s + 3) : o;
  int c = oh.indexOf(':');
  if (c >= 0) oh = oh.substring(0, c);
  int sl = oh.indexOf('/');
  if (sl >= 0) oh = oh.substring(0, sl);
  return oh == host;
}

static bool needAuth(int minimum) {
  int r = currentRole();
  if (r >= minimum && r != R_NONE) {
    if (s_server.method() == HTTP_POST && !originOk()) {
      sendSecure(403, "text/plain", "Bad origin");
      return true;
    }
    return false;
  }
  String uri = s_server.uri();
  if (uri.startsWith("/api/")) {
    sendSecure(401, "application/json", "{\"error\":\"login\"}");
  } else {
    s_server.sendHeader("Location", "/login");
    sendSecure(302, "text/plain", "Login required");
  }
  return true;
}

static void setSessionCookie(const std::string& token) {
  String v = "HA_SESSION=" + String(token.c_str()) +
             "; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800";
  s_server.sendHeader("Set-Cookie", v);
}

static void clearSessionCookie() {
  s_server.sendHeader("Set-Cookie", "HA_SESSION=; Path=/; Max-Age=0");
}

// First-boot gate: force admin password setup before anything else.
static bool setupGate() {
  if (!s_web->auth->adminMustChange()) return false;
  String uri = s_server.uri();
  if (uri == "/setup" || uri == "/api/setup" || uri == "/api/state")
    return false;
  s_server.sendHeader("Location", "/setup");
  sendSecure(302, "text/plain", "Setup required");
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
                "<a href='/update'>OTA</a><a href='#' onclick=\"fetch('/api/logout',"
                "{method:'POST'}).then(()=>location='/login');return false\">"
                "Logout</a></nav><h1>" +
                title + "</h1>" + body + "</main></body></html>";
  sendSecure(200, "text/html", html);
}

static void handleRoot() {
  if (setupGate()) return;
  if (needAuth(R_USER)) return;
  String body =
      "<div class='card'><div id='st'>loading…</div></div>"
      "<div class='card' id='relays'></div>"
      "<script>"
      "async function st(){const r=await fetch('/api/state');"
      "if(r.status===401){location='/login';return;}"
      "const j=await r.json();"
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
    sendSecure(302, "text/plain", "Already set up");
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
    sendSecure(403, "text/plain", "Already set up");
    return;
  }
  String pw = s_server.arg("password");
  if (pw.length() < 8) {
    sendSecure(400, "text/plain", "Password must be >= 8 characters");
    return;
  }
  s_web->auth->setAdminPassword(pw.c_str());
  if (s_web->otaPassword->empty()) *s_web->otaPassword = pw.c_str();
  s_web->saveAll();
  s_web->addLog("Admin password set (first boot)");
  sendSecure(200, "text/plain", "OK - now log in as admin");
}

static void handleLoginPage() {
  if (!s_web->auth->adminMustChange() && currentRole() != R_NONE) {
    s_server.sendHeader("Location", "/");
    sendSecure(302, "text/plain", "Already in");
    return;
  }
  String body =
      "<div class='card'><label>User<select id='u'>"
      "<option value='admin'>admin</option>"
      "<option value='user'>user</option></select></label>"
      "<label>Password<input type='password' id='p'></label>"
      "<button onclick='li()'>Log in</button><p id='m'></p></div>"
      "<script>async function li(){const f=new URLSearchParams({user:"
      "document.getElementById('u').value,password:document.getElementById('p')."
      "value});const r=await fetch('/api/login',{method:'POST',body:f});"
      "if(r.ok)location='/';else "
      "document.getElementById('m').innerText=await r.text();}</script>";
  page("Log in", body);
}

static void handleApiLogin() {
  if (!originOk()) {
    sendSecure(403, "text/plain", "Bad origin");
    return;
  }
  std::string ip = clientIp();
  uint32_t now = millis();
  // 5 failures in 15 min -> 5 min lockout (brute-force guard).
  if (s_guard.isLocked(ip, now)) {
    sendSecure(429, "text/plain", "Locked out - try again later");
    return;
  }
  String user = s_server.arg("user");
  String pw = s_server.arg("password");
  int role = R_NONE;
  if (pw.length() <= 64) {  // length cap: no 10 MB hash jobs
    if (user == "admin" && s_web->auth->verifyAdmin(pw.c_str()))
      role = R_ADMIN;
    else if (user == "user" && s_web->auth->verifyUser(pw.c_str()))
      role = R_USER;
  }
  if (role == R_NONE) {
    bool locked = s_guard.noteFail(ip, now);
    char msg[96];
    snprintf(msg, sizeof(msg), "Failed login as '%s' from %s%s", user.c_str(),
             ip.c_str(), locked ? " (IP locked)" : "");
    s_web->addLog(msg);
    sendSecure(locked ? 429 : 401, "text/plain",
               locked ? "Locked out - try again later" : "Bad login");
    return;
  }
  s_guard.noteSuccess(ip);
  std::string token = s_sessions.login(
      role == R_ADMIN ? ha::kRoleAdmin : ha::kRoleUser, now);
  if (token.empty()) {
    sendSecure(503, "text/plain", "Session table full - retry shortly");
    return;
  }
  setSessionCookie(token);
  sendSecure(200, "application/json",
             role == R_ADMIN ? "{\"ok\":1,\"role\":\"admin\"}"
                             : "{\"ok\":1,\"role\":\"user\"}");
}

static void handleApiLogout() {
  s_sessions.logout(sessionToken());
  clearSessionCookie();
  sendSecure(200, "text/plain", "Logged out");
}

// OTA download URLs must point at GitHub (blocks SSRF to the LAN via a
// crafted admin request or a compromised release JSON).
static bool ghUrlAllowed(const std::string& url) {
  static const char* kPrefix = "https://";
  if (url.compare(0, 8, kPrefix) != 0) return false;
  size_t end = url.find('/', 8);
  std::string host =
      url.substr(8, end == std::string::npos ? end : end - 8);
  return host == "github.com" || host == "objects.githubusercontent.com" ||
         host == "api.github.com";
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
  sendSecure(200, "application/json", stateJson());
}

static void applyRelay(int ch, bool on, const char* src) {
  if (!s_web->relays->set(ch, on)) {
    sendSecure(400, "text/plain", "Invalid channel");
    return;
  }
  digitalWrite(s_web->relayPins[ch], s_web->relays->levelForChannel(ch));
  char msg[64];
  snprintf(msg, sizeof(msg), "Relay %d %s (%s)", ch + 1, on ? "ON" : "OFF", src);
  s_web->addLog(msg);
  sendSecure(200, "application/json",
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
    sendSecure(400, "text/plain", "Invalid relay index");
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
  sendSecure(200, "text/plain", out);
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
    sendSecure(400, "text/plain", "Min 8 characters");
    return;
  }
  s_web->auth->setAdminPassword(pw.c_str());
  s_web->saveAll();
  s_web->addLog("Admin password changed");
  sendSecure(200, "text/plain", "Admin password changed");
}

static void handleUserSet() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  bool en = s_server.arg("enabled") == "1";
  String pw = s_server.arg("password");
  if (en && pw.length() < 4) {
    sendSecure(400, "text/plain", "User password min 4 characters");
    return;
  }
  s_web->auth->setUserEnabled(en);
  if (pw.length()) s_web->auth->setUserPassword(pw.c_str());
  s_web->saveAll();
  s_web->addLog(en ? "User account enabled" : "User account disabled");
  sendSecure(200, "text/plain", "User settings saved");
}

static void handleOtaPw() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String pw = s_server.arg("password");
  if (pw.length() < 8) {
    sendSecure(400, "text/plain", "Min 8 characters");
    return;
  }
  *s_web->otaPassword = pw.c_str();
  s_web->saveAll();
  s_web->addLog("OTA password changed (applies after reboot)");
  sendSecure(200, "text/plain", "OTA password saved - reboot to apply");
}

static void handlePortal() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  sendSecure(200, "text/plain", "Opening portal…");
  delay(500);
  s_web->requestPortal();
}

static void handleRestart() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  sendSecure(200, "text/plain", "Rebooting…");
  delay(500);
  s_web->requestReboot();
}

static void handleUpdatePage() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  String body =
      "<div class='card'><h3>Automatic update (GitHub release)</h3>"
      "<p>Running: <b>" +
      String(s_web->fwVersion) +
      "</b> <span id='g'>…checking…</span></p>"
      "<button onclick='ghcheck()'>Check for updates</button> "
      "<span id='gi'></span><p id='gm'></p></div>"
      "<div class='card'><h3>Manual upload</h3><p>Upload a compiled .bin. "
      "Device reboots automatically.</p>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'><button type='submit'>Upload</button>"
      "</form></div>"
      "<script>async function ghcheck(){document.getElementById('g').innerText='checking…';"
      "const r=await fetch('/api/ghcheck');const j=await r.json();"
      "document.getElementById('g').innerText='latest: '+(j.tag||'check failed');"
      "document.getElementById('gi').innerHTML=j.newer?'<button onclick=\"ghup(\\''+j.url+'\\')\">Install '+j.tag+'</button>':'';}"
      "async function ghup(u){if(!confirm('Flash '+u+'?'))return;"
      "document.getElementById('gm').innerText='downloading… (up to a minute)';"
      "const f=new URLSearchParams({url:u});"
      "const r=await fetch('/api/ghupdate',{method:'POST',body:f});"
      "document.getElementById('gm').innerText=await r.text();}"
      "ghcheck();</script>";
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
    sendSecure(403, "text/plain", "Update failed or not authorized");
    return;
  }
  sendSecure(200, "text/plain", "Update OK - rebooting…");
  delay(500);
  s_web->requestReboot();  // via main: persists + disarms reset detector
}

static void handleApiGhCheck() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  std::string tag, url;
  bool ok = ghCheckLatest(tag, url);
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"current\":\"" + String(s_web->fwVersion) + "\"";
  j += ",\"tag\":\"" + String(tag.c_str()) + "\"";
  j += ",\"url\":\"" + String(url.c_str()) + "\"";
  j += ",\"newer\":";
  j += (ok && ha::ghota::updateAvailable(s_web->fwVersion, tag)) ? "true"
                                                                 : "false";
  j += "}";
  sendSecure(200, "application/json", j);
}

static void handleApiGhUpdate() {
  if (setupGate()) return;
  if (needAuth(R_ADMIN)) return;
  std::string url = s_server.arg("url").c_str();
  if (url.empty() || !ghUrlAllowed(url)) {
    sendSecure(400, "text/plain", "URL not allowed");
    return;
  }
  auto logFn = [](const std::string& m) { s_web->addLog(m); };
  s_server.sendHeader("Connection", "close");
  if (!ghDownloadAndFlash(url, logFn)) {
    sendSecure(500, "text/plain", "Download/flash failed - see log");
    return;
  }
  sendSecure(200, "text/plain", "Update OK - rebooting…");
  delay(500);
  s_web->requestReboot();
}

static void handleNotFound() {
  // Legacy relay path with manual parse (core 3.1.x has no UriBraces).
  String uri = s_server.uri();
  if (s_server.method() == HTTP_POST && uri.startsWith("/relay/")) {
    handleRelayToggleLegacy(uri.substring(7).toInt());
    return;
  }
  sendSecure(404, "text/plain", "Not found");
}

void webBegin(AppContext* ctx) {
  s_web = ctx;
  s_sessions.configure(8, 1800000);  // 8 sessions, 30 min sliding expiry
  s_sessions.setRng([]() -> uint32_t { return ESP.random(); });
  s_guard.configure(5, 900000, 300000, 16);  // 5 fails/15 min -> 5 min lock
  // This core's collectHeaders() is variadic-template-only: array form
  // resolves to the template and fails to compile. Pass headers directly.
  s_server.collectHeaders("Cookie", "Origin");
  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/login", HTTP_GET, handleLoginPage);
  s_server.on("/api/login", HTTP_POST, handleApiLogin);
  s_server.on("/api/logout", HTTP_POST, handleApiLogout);
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
  s_server.on("/api/ghcheck", HTTP_GET, handleApiGhCheck);
  s_server.on("/api/ghupdate", HTTP_POST, handleApiGhUpdate);
  s_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_server.onNotFound(handleNotFound);
  s_server.begin();
}

void webLoop() {
  s_server.handleClient();
  uint32_t now = millis();
  if ((uint32_t)(now - s_lastSweep) >= 60000) {
    s_lastSweep = now;
    s_sessions.sweep(now);  // drop expired sessions once a minute
  }
}
