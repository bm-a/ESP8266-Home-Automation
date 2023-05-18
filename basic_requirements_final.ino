#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <MD5Builder.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

const char* ssid = "Hostel";
const char* password = "";

const char* adminUsername = "bhavishya";
const char* adminPassword = "madan123";
const char* userUsername = "divyansh";
const char* userPassword = "khandekar123";

const int relayPin1 = 1;
const int relayPin2 = 2;
const int relayPin3 = 3;
const int relayPin4 = 4;

const int switchPin1 = 5;
const int switchPin2 = 6;
const int switchPin3 = 7;
const int switchPin4 = 8;

const int logRetentionPeriod = 30; // Retain logs for 30 days

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

bool isAuthenticated = false;
bool isAdmin = false;
int relay1State = LOW;
int relay2State = LOW;
int relay3State = LOW;
int relay4State = LOW;
unsigned long lastSendDataTime = 0;
unsigned long sendDataInterval = 5 * 60 * 1000; // 5 minutes

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
  html += "<a class='relay-link' href='/relay?state=" + String(relay4State == HIGH ? 0 : 1) + "&id=4'>Turn Relay 4 " + String(relay4State == HIGH ? "Off" : "On") + "</a><br/>";
  html += "</div>";
  html += "<h2>Logs</h2>";
  html += "<div id='log-container'>" + logContent + "</div>";
  html += "</body>";
  html += "<script>";
  html += "var relayLinks = document.getElementsByClassName('relay-link');";
  html += "for (var i = 0; i < relayLinks.length; i++) {";
  html += "  relayLinks[i].onclick = function(event) {";
  html += "    event.preventDefault();";
  html += "    var href = this.href;";
  html += "    var xhr = new XMLHttpRequest();";
  html += "    xhr.onreadystatechange = function() {";
  html += "      if (xhr.readyState === XMLHttpRequest.DONE) {";
  html += "        if (xhr.status === 200) {";
  html += "          location.reload();";
  html += "        } else {";
  html += "          alert('Failed to toggle relay.');";
  html += "        }";
  html += "      }";
  html += "    };";
  html += "    xhr.open('GET', href, true);";
  html += "    xhr.send();";
  html += "  };";
  html += "}";
  html += "</script>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleRelay() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "");
    return;
  }

  String relayStateParam = server.arg("state");
  String relayIdParam = server.arg("id");

  if (relayStateParam.length() > 0 && relayIdParam.length() > 0) {
    int relayState = relayStateParam.toInt();
    int relayId = relayIdParam.toInt();

    if (relayId == 1) {
      digitalWrite(relayPin1, relayState);
      relay1State = relayState;
    } else if (relayId == 2) {
      digitalWrite(relayPin2, relayState);
      relay2State = relayState;
    } else if (relayId == 3) {
      digitalWrite(relayPin3, relayState);
      relay3State = relayState;
    } else if (relayId == 4) {
      digitalWrite(relayPin4, relayState);
      relay4State = relayState;
    }

    String response = "Relay " + String(relayId) + " turned " + String(relayState == HIGH ? "On" : "Off") + ".";
    server.send(200, "text/plain", response);

    logContent += "<p>" + response + "</p>";
    File logFile = SPIFFS.open(getCurrentLogFilename(), "a");
    if (logFile) {
      logFile.println(response);
      logFile.close();
    }
  } else {
    server.send(400, "text/plain", "Invalid request.");
  }
}

void handleLogin() {
  if (isAuthenticated) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }

  if (server.method() == HTTP_GET) {
    String html = "<html><head><title>Login</title></head><body>";
    html += "<h1>Login</h1>";
    html += "<form method='POST'>";
    html += "<label for='username'>Username:</label><br/>";
    html += "<input type='text' id='username' name='username'><br/>";
    html += "<label for='password'>Password:</label><br/>";
    html += "<input type='password' id='password' name='password'><br/>";
    html += "<input type='submit' value='Login'>";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  } else if (server.method() == HTTP_POST) {
    String username = server.arg("username");
    String password = md5Digest(server.arg("password"));

    if ((username == adminUsername && password == md5Digest(adminPassword)) ||
        (username == userUsername && password == md5Digest(userPassword))) {
      isAuthenticated = true;
      isAdmin = (username == adminUsername);

      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.sendHeader("Location", "/login", true);
      server.send(302, "text/plain", "");
    }
  }
}

void handleLogout() {
  isAuthenticated = false;
  isAdmin = false;

  server.sendHeader("Location", "/login", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", message);
}

void updateLogs() {
File root = SPIFFS.open("/", "r");

  File file = root.openNextFile();

  logContent = "<h3>Logs</h3>";

  while (file) {
    String filename = file.name();
    if (filename.startsWith("/log_") && filename.endsWith(".txt")) {
      logContent += "<h4>" + filename.substring(5, 13) + "</h4>";
      File logFile = SPIFFS.open(filename, "r");
      if (logFile) {
        String line;
        while (logFile.available()) {
          line = logFile.readStringUntil('\n');
          logContent += "<p>" + line + "</p>";
        }
        logFile.close();
      }
    }
    file = root.openNextFile();
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.println(WiFi.localIP());

  // Initialize the web server
  server.on("/", handleRoot);
  server.on("/relay", handleRelay);
  server.on("/login", handleLogin);
  server.on("/logout", handleLogout);
  server.onNotFound(handleNotFound);

  // Start the web server
  server.begin();

  // Initialize the relay pins
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  // Initialize the switch pins
  pinMode(switchPin1, INPUT_PULLUP);
  pinMode(switchPin2, INPUT_PULLUP);
  pinMode(switchPin3, INPUT_PULLUP);
  pinMode(switchPin4, INPUT_PULLUP);

  // Initialize the time client
  timeClient.begin();
  timeClient.update();

  // Set the initial relay states
  relay1State = digitalRead(relayPin1);
  relay2State = digitalRead(relayPin2);
  relay3State = digitalRead(relayPin3);
  relay4State = digitalRead(relayPin4);

  // Load and update the logs
  updateLogs();
}

void loop() {
  // Handle client requests
  server.handleClient();

  // Update relay states based on switch input
  int switchState1 = digitalRead(switchPin1);
  int switchState2 = digitalRead(switchPin2);
  int switchState3 = digitalRead(switchPin3);
  int switchState4 = digitalRead(switchPin4);

  if (switchState1 == LOW) {
    digitalWrite(relayPin1, HIGH);
    relay1State = HIGH;
  } else {
    digitalWrite(relayPin1, LOW);
    relay1State = LOW;
  }

  if (switchState2 == LOW) {
    digitalWrite(relayPin2, HIGH);
    relay2State = HIGH;
  } else {
    digitalWrite(relayPin2, LOW);
    relay2State = LOW;
  }

  if (switchState3 == LOW) {
    digitalWrite(relayPin3, HIGH);
    relay3State = HIGH;
  } else {
    digitalWrite(relayPin3, LOW);
    relay3State = LOW;
  }

  if (switchState4 == LOW) {
    digitalWrite(relayPin4, HIGH);
    relay4State = HIGH;
  } else {
    digitalWrite(relayPin4, LOW);
    relay4State = LOW;
  }

  // Update the time client
  if (millis() - lastSendDataTime > sendDataInterval) {
    timeClient.update();
    lastSendDataTime = millis();
  }

  // Check if it's a new day and update logs
  if (day() != day(timeClient.getEpochTime())) {
    updateLogs();
  }
}
