#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoOTA.h> // Added library for OTA updates

// Pin definitions
const int RELAY_PINS[] = {1, 2, 3, 4};
const int SWITCH_PINS[] = {5, 6, 7, 8};

// Wi-Fi settings
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* HOTSPOT_SSID = "esp";
const char* HOTSPOT_PASSWORD = "esp";

// NTP settings
const char* NTP_SERVER = "in.pool.ntp.org";
const int NTP_UPDATE_INTERVAL = 300; // seconds

// Log file settings
const char* LOG_FILENAME = "/log.txt";
const int MAX_LOG_AGE_DAYS = 30;

// Web server settings
const int HTTP_PORT = 80;
const char* ADMIN_USERNAME = "admin";
const char* ADMIN_PASSWORD = "admin";
const char* USER_USERNAME = "user";
const char* USER_PASSWORD = "user";

// Variables
ESP8266WebServer server(HTTP_PORT);
WiFiUDP udp;
unsigned long lastNtpUpdate = 0;
NTPClient timeClient(udp, NTP_SERVER, 0, NTP_UPDATE_INTERVAL * 1000);

// Helper function to toggle relay state
void toggleRelay(int pin) {
  digitalWrite(pin, !digitalRead(pin));
}

// Helper function to delete old logs
void deleteOldLogs() {
  if (!SPIFFS.exists(LOG_FILENAME)) {
    return;
  }

  File logFile = SPIFFS.open(LOG_FILENAME, "r");
  if (!logFile) {
    return;
  }

  String logContent = logFile.readString();
  logFile.close();

  File newLogFile = SPIFFS.open(LOG_FILENAME, "w");
  if (!newLogFile) {
    return;
  }

  newLogFile.print(logContent);
  newLogFile.close();
}

// Helper function to write log entry
void writeLog(const String& message) {
  // Get the current time
  time_t now = timeClient.getEpochTime();
  
  // Format the timestamp
  char timestamp[20];
  sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d]", year(now), month(now), day(now), hour(now), minute(now), second(now));

  // Create the log entry with the timestamp
  String logEntry = String(timestamp) + " " + message + "\n";
  
  // Open the log file in append mode
  File logFile = SPIFFS.open(LOG_FILENAME, "a");
  
  // If the file is available, write the log entry
  if (logFile) {
    logFile.print(logEntry);
    logFile.close();
  }
}

// Web server handlers

void handleRoot() {
  // Display main web page
  String html = "<html><head><style>"
                "body{background-color:#000;font-family:'Courier New', monospace;color:#fff;margin:0;padding:0}"
                ".container{max-width:800px;margin:0 auto;padding:20px}"
                "h1{font-size:24px}"
                "button{background-color:#2ecc40;color:#000;border:none;padding:10px 20px;font-size:16px;cursor:pointer;margin-top:10px}"
                "input[type='file']{margin-top:10px}"
                ".log-container{border: 1px solid #fff;padding: 10px;margin-top: 20px;max-height: 200px;overflow-y: auto;}"
                ".log-entry{font-size: 14px;}"
                "</style></head><body>"
                "<div class='container'>";

  // Add toggle buttons for relays
  for (int i = 0; i < sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]); i++) {
    html += "<h1>Relay " + String(i + 1) + "</h1>";
    html += "<button onclick='toggleRelay(" + String(i) + ")'>Toggle</button><br>";
  }

  // Add form to change Wi-Fi credentials (for admin)
  html += "<h1>Wi-Fi Credentials</h1>";
  html += "<form method='POST' action='/wifi' enctype='multipart/form-data'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<button type='submit'>Save</button>";
  html += "</form>";

  // Add form to change hotspot settings (for admin)
  html += "<h1>Hotspot Settings</h1>";
  html += "<form method='POST' action='/hotspot' enctype='multipart/form-data'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<button type='submit'>Save</button>";
  html += "</form>";

  // Display login page for "Hostel" Wi-Fi network
  html += "<h1>Login</h1>";
  html += "<iframe src='http://192.168.1.1'></iframe>";

  // Display logs (for admin)
  if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    html += "<div class='log-container'>Logs:<br>";
    File logFile = SPIFFS.open(LOG_FILENAME, "r");
    if (logFile) {
      while (logFile.available()) {
        String logEntry = logFile.readStringUntil('\n');
        html += "<div class='log-entry'>" + logEntry + "</div>";
      }
      logFile.close();
    }
    html += "</div>";
  }

  html += "</div><script>"
          "function toggleRelay(index) {"
          "  var xhr = new XMLHttpRequest();"
          "  xhr.open('POST', '/relay/' + index, true);"
          "  xhr.send();"
          "}"
          "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleToggleRelay() {
  int index = server.pathArg(0).toInt();
  if (index >= 0 && index < sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0])) {
    if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD) || server.authenticate(USER_USERNAME, USER_PASSWORD)) {
      toggleRelay(RELAY_PINS[index]);
      String message = "Toggled Relay " + String(index + 1);
      writeLog(message);
      server.send(200, "text/plain", message);
    } else {
      server.requestAuthentication();
    }
  } else {
    server.send(400, "text/plain", "Invalid relay index");
  }
}


void handleWiFi() {
  if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    // TODO: Update Wi-Fi credentials
    // For ESP8266, you can use WiFi.begin(ssid, password) to update the credentials

    String message = "Changed Wi-Fi credentials";
    writeLog(message);
    server.send(200, "text/plain", message);
  } else {
    server.requestAuthentication();
  }
}

void handleHotspot() {
  if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    // TODO: Update hotspot settings

    String message = "Changed hotspot settings";
    writeLog(message);
    server.send(200, "text/plain", message);
  } else {
    server.requestAuthentication();
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// OTA update handlers

void handleOTAUpload() {
  if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "Update failed" : "Update success");
    ESP.restart();
  } else {
    server.requestAuthentication();
  }
}

void handleOTARestart() {
  if (server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    server.send(200, "text/plain", "Restarting...");
    ESP.restart();
  } else {
    server.requestAuthentication();
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to initialize SPIFFS");
    return;
  }

  // Initialize Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to Wi-Fi, IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize NTP client
  timeClient.begin();

  // Initialize relay pins
  for (int i = 0; i < sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]); i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // Initialize switch pins
  for (int i = 0; i < sizeof(SWITCH_PINS) / sizeof(SWITCH_PINS[0]); i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }

  // Start web server
  server.on("/", handleRoot);
  server.on("/relay/<index>", handleToggleRelay);
  server.on("/wifi", handleWiFi);
  server.on("/hotspot", handleHotspot);
  server.on("/ota/upload", HTTP_POST, handleOTAUpload); // OTA firmware upload endpoint
  server.on("/ota/restart", handleOTARestart); // OTA restart endpoint
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started");

  // Start OTA updates
  ArduinoOTA.begin();

  // Delete old logs
  deleteOldLogs();
}

void loop() {
  // Handle web server requests
  server.handleClient();

  // Handle OTA updates
  ArduinoOTA.handle();

  // Update NTP time
  if (millis() - lastNtpUpdate >= NTP_UPDATE_INTERVAL * 1000) {
    timeClient.update();
    lastNtpUpdate = millis();
  }

  // Check switch states
  for (int i = 0; i < sizeof(SWITCH_PINS) / sizeof(SWITCH_PINS[0]); i++) {
    if (digitalRead(SWITCH_PINS[i]) == LOW) {
      String message = "Switch " + String(i + 1) + " is pressed";
      writeLog(message);
    }
  }
}
