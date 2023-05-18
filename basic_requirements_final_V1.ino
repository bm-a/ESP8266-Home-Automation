#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <MD5Builder.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

const char* ssid = "";
const char* password = "";

const char* adminUsername = "";
const char* adminPassword = "";
const char* userUsername = "";
const char* userPassword = "";

const int relayPin1 = 1;
const int relayPin2 = 2;
const int relayPin3 = 3;
const int relayPin4 = 4;

const int switchPin1 = 5;
const int switchPin2 = 6;
const int switchPin3 = 7;
const int switchPin4 = 8;

const int logRetentionPeriod = 30;

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org");

bool isAuthenticated = false;
bool isAdmin = false;
int relay1State = LOW;
int relay2State = LOW;
int relay3State = LOW;
int relay4State = LOW;
unsigned long lastSendDataTime = 0;
unsigned long sendDataInterval = 5 * 60 * 1000;

String logContent = "";

String md5Digest(const String& str) {
  MD5Builder md5;
  md5.begin();
  md5.add(str);
  md5.calculate();
  return md5.toString();
}

String getCurrentLogFilename() {
  time_t currentTime = now();
  char filename[20];
  sprintf(filename, "/log_%04d%02d%02d.txt", year(currentTime), month(currentTime), day(currentTime));
  return String(filename);
}

void deleteOldLogFiles() {
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String filename = dir.fileName();
    if (filename.startsWith("/log_") && filename.endsWith(".txt")) {
      int year, month, day;
      sscanf(filename.c_str(), "/log_%04d%02d%02d.txt", &year, &month, &day);
      time_t fileTime = 0;
      setTime(0, 0, 0, day, month, year); // Set the date and time components
      fileTime = now(); // Convert to a time_t value

      time_t currentTime = now();
      time_t retentionTime = currentTime - (logRetentionPeriod * 24 * 60 * 60);
      if (fileTime < retentionTime) {
        SPIFFS.remove(filename);
        Serial.println("Deleted log file: " + filename);
      }
    }
  }
}


void handleRoot() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
    return;
  }
  
  String html = "<html><head><title>Home</title></head><body>";
  html += "<h1>Welcome, " + String(isAdmin ? adminUsername : userUsername) + "!</h1>";
  html += "<h2>Relay Control</h2>";
  html += "<div>";
  html += "<a class='relay-link' href='/relay?state=" + String(relay1State == HIGH ? 0 : 1) + "&id=1'>Turn Relay 1 " + String(relay1State == HIGH ? "Off" : "On") + "</a><br/>";
  html += "<a class='relay-link' href='/relay?state=" + String(relay2State == HIGH ? 0 : 1) + "&id=2'>Turn Relay 2 " + String(relay2State == HIGH ? "Off" : "On") + "</a><br/>";
  html += "<a class='relay-link' href='/relay?state=" + String(relay3State == HIGH ? 0 : 1) + "&id=3'>Turn Relay 3 " + String(relay3State == HIGH ? "Off" : "On") + "</a><br/>";
  html += "<a class='relay-link' href='/relay?state=" + String(relay4State == HIGH ? 0 : 1) + "&id=4'>Turn Relay 4 " + String(relay4State == HIGH ? "Off" : "On") + "</a>";
  html += "</div>";
  html += "<script>";
  html += "var links = document.getElementsByClassName('relay-link');";
  html += "for (var i = 0; i < links.length; i++) {";
  html += "  links[i].addEventListener('click', function(event) {";
  html += "    event.preventDefault();";
  html += "    var url = this.href;";
  html += "    var xhr = new XMLHttpRequest();";
  html += "    xhr.open('GET', url, true);";
  html += "    xhr.send();";
  html += "  });";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleLogin() {
  String html = "<html><head><title>Login</title></head><body>";
  html += "<h1>Login</h1>";
  html += "<form method='post' action='/login'>";
  html += "Username: <input type='text' name='username'><br/>";
  html += "Password: <input type='password' name='password'><br/>";
  html += "<input type='submit' value='Login'>";
  html += "</form>";
  html += "</body></html>";
  
  if (server.method() == HTTP_POST) {
    String username = server.arg("username");
    String password = md5Digest(server.arg("password"));
    if ((username == adminUsername && password == adminPassword) || (username == userUsername && password == userPassword)) {
      isAuthenticated = true;
      isAdmin = (username == adminUsername);
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
    
    html += "<p>Invalid username or password!</p>";
  }
  
  server.send(200, "text/html", html);
}

void handleRelay() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
    return;
  }
  
  int relayId = server.arg("id").toInt();
  int relayState = server.arg("state").toInt();
  
  switch (relayId) {
    case 1:
      digitalWrite(relayPin1, relayState == HIGH ? HIGH : LOW);
      relay1State = relayState;
      break;
    case 2:
      digitalWrite(relayPin2, relayState == HIGH ? HIGH : LOW);
      relay2State = relayState;
      break;
    case 3:
      digitalWrite(relayPin3, relayState == HIGH ? HIGH : LOW);
      relay3State = relayState;
      break;
    case 4:
      digitalWrite(relayPin4, relayState == HIGH ? HIGH : LOW);
      relay4State = relayState;
      break;
    default:
      break;
  }
  
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleLog() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
    return;
  }
  
  String html = "<html><head><title>Log</title></head><body>";
  html += "<h1>Log</h1>";
  
  File logFile = SPIFFS.open(getCurrentLogFilename(), "r");
  if (logFile) {
    while (logFile.available()) {
      html += logFile.readStringUntil('\n');
      html += "<br/>";
    }
    logFile.close();
  } else {
    html += "<p>No log available.</p>";
  }
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  pinMode(switchPin1, INPUT_PULLUP);
  pinMode(switchPin2, INPUT_PULLUP);
  pinMode(switchPin3, INPUT_PULLUP);
  pinMode(switchPin4, INPUT_PULLUP);

  digitalWrite(relayPin1, relay1State);
  digitalWrite(relayPin2, relay2State);
  digitalWrite(relayPin3, relay3State);
  digitalWrite(relayPin4, relay4State);

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  SPIFFS.begin();

  deleteOldLogFiles(); // Add this line to delete old log files

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  timeClient.begin();

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/relay", handleRelay);
  server.on("/log", handleLog);

  server.begin();
}

void loop() {
  MDNS.update();
  timeClient.update();
  server.handleClient();

  if (millis() - lastSendDataTime >= sendDataInterval) {
    lastSendDataTime = millis();
    String filename = getCurrentLogFilename();
    File file = SPIFFS.open(filename, "a+");
    if (file) {
      file.println(logContent);
      file.close();
      logContent = "";
    }
  }
}
