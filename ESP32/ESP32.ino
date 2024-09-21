#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include <Keypad.h>
#include <Ticker.h>
#include <SPIFFS.h>

// Configuration constants
const char* ssid = "****";
const char* password = "****";

const char* deviceName = "GateController";

// NTP server settings
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";

// Pin definitions
#define RELAY_OPEN 12
#define RELAY_STOP 33
#define RELAY_LIGHT 14
#define GATE_OPEN 35
#define GATE_OPEN_LED 26
#define GATE_CLOSE 34
#define GATE_CLOSE_LED 25
#define CONNECTION_LED 27

// Global variables
WebServer server(80);
bool lightEnabled = false;
int lightOnTime = 0;
int lightOffTime = 0;
int lightDuration = 10;
char gatePassword[20] = "";
char gateBehavior[10] = "";
bool timeInitialized = false;

unsigned long lastTimeSyncMillis = 0;
const unsigned long timeSyncInterval = 3600000; // 1 hour
unsigned long lastWiFiCheckMillis = 0;
const unsigned long wifiCheckInterval = 30000; // 30 seconds
unsigned long lastRebootMillis = 0;
const unsigned long rebootInterval = 7 * 24 * 60 * 60 * 1000; // 7 days
unsigned long lastKeyPressTime = 0;
const unsigned long keyPressTimeout = 5000;

#define LOG_FILE "/gate_log.txt"
#define MAX_LOG_AGE 1728000000 // 20 days in milliseconds

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 4, 16, 17, 23 };
byte colPins[COLS] = { 18, 19, 21, 22 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
String enteredCode = "";

Ticker lightTicker;

// Function prototypes
void setupWiFi();
void configureTime();
void loadSettings();
void saveSettings();
void handleRoot();
void handleOpen();
void handleStop();
void handleStayOpen();
void handleSetSettings();
void handleGetSettings();
void handleGetStatus();
void handleGetLogs();
void handleGetSystemInfo();
void syncTimeIfNeeded();
void checkWiFiConnection();
void checkForReboot();
void handleKeypad();
void updateGateLedStatus();
void turnOffLight();
void performGateAction();
void activateRelay(int pin, int duration);
void logGateAction(const char* action, String ipAddress);
void logError(const char* errorMessage);
void resetLogs();
void rebootDevice();
bool isTimeWithinRange();
int getCurrentTime();
bool handleFileRead(String path);
String getContentType(String filename);
void sendJsonResponse(int code);

// Setup function
void setup() {
  if (!SPIFFS.begin(true)) {
    return;
  }

  // Set pin modes and initialize
  pinMode(RELAY_OPEN, OUTPUT);
  pinMode(RELAY_STOP, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(GATE_OPEN, INPUT);
  pinMode(GATE_OPEN_LED, OUTPUT);
  pinMode(GATE_CLOSE, INPUT);
  pinMode(GATE_CLOSE_LED, OUTPUT);
  pinMode(CONNECTION_LED, OUTPUT);

  digitalWrite(RELAY_OPEN, LOW);
  digitalWrite(RELAY_STOP, LOW);
  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(CONNECTION_LED, LOW);

  for (byte i = 0; i < COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  for (byte i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }

  setupWiFi();
  EEPROM.begin(512);
  loadSettings();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/open", HTTP_GET, handleOpen);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/stayopen", HTTP_GET, handleStayOpen);
  server.on("/setsettings", HTTP_POST, handleSetSettings);
  server.on("/getsettings", HTTP_GET, handleGetSettings);
  server.on("/getstatus", HTTP_GET, handleGetStatus);
  server.on("/getlogs", HTTP_GET, handleGetLogs);
  server.on("/getsysteminfo", HTTP_GET, handleGetSystemInfo);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "File Not Found");
  });

  server.begin();
  lastRebootMillis = millis();
}

// Main loop
void loop() {
  try {
    server.handleClient();
    syncTimeIfNeeded();
    handleKeypad();
    updateGateLedStatus();
    checkWiFiConnection();
    checkForReboot();
  } catch (const std::exception& e) {
    logError("Critical error in main loop. Rebooting...");
    delay(1000);
    rebootDevice();
  }
}

// WiFi and Time functions
// -----------------------

// Set up WiFi connection
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName);
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    digitalWrite(CONNECTION_LED, !digitalRead(CONNECTION_LED));
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(CONNECTION_LED, HIGH);
    delay(1000);
    configureTime();
  } else {
    digitalWrite(CONNECTION_LED, LOW);
  }
}

// Configure and synchronize time
void configureTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  
  int retryCount = 0;
  const int maxRetries = 5;
  
  while (!timeInitialized && retryCount < maxRetries) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      timeInitialized = true;
      logGateAction("Time synchronized", WiFi.localIP().toString());
    } else {
      retryCount++;
      delay(1000);
    }
  }
  
  if (!timeInitialized) {
    logError("Failed to obtain time after multiple attempts");
  }
}

// Check and maintain WiFi connection
void checkWiFiConnection() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastWiFiCheckMillis >= wifiCheckInterval) {
    lastWiFiCheckMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      logError("WiFi disconnected. Attempting to reconnect...");
      setupWiFi();
    }
  }
}

// Synchronize time if needed
void syncTimeIfNeeded() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastTimeSyncMillis >= timeSyncInterval || !timeInitialized) {
    configureTime();
    lastTimeSyncMillis = currentMillis;
  }
}

// Check if it's time for a scheduled reboot
void checkForReboot() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastRebootMillis >= rebootInterval) {
    logGateAction("Performing scheduled reboot", WiFi.localIP().toString());
    delay(1000);
    rebootDevice();
  }
}

// Restart the device
void rebootDevice() {
  ESP.restart();
}

// Settings functions
// ------------------

// Load settings from EEPROM
void loadSettings() {
  EEPROM.get(0, lightEnabled);
  EEPROM.get(1, lightDuration);
  EEPROM.get(5, lightOnTime);
  EEPROM.get(9, lightOffTime);
  EEPROM.get(13, gatePassword);
  EEPROM.get(33, gateBehavior);
}

// Save settings to EEPROM
void saveSettings() {
  EEPROM.put(0, lightEnabled);
  EEPROM.put(1, lightDuration);
  EEPROM.put(5, lightOnTime);
  EEPROM.put(9, lightOffTime);
  EEPROM.put(13, gatePassword);
  EEPROM.put(33, gateBehavior);
  EEPROM.commit();
}

// Server handler functions
// ------------------------

// Handle root path
void handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

// Handle open gate request
void handleOpen() {
  activateRelay(RELAY_OPEN, 1000);

  if (lightEnabled && isTimeWithinRange()) {
    digitalWrite(RELAY_LIGHT, HIGH);
    lightTicker.attach_ms(lightDuration * 1000, turnOffLight);
  }

  logGateAction("Open", server.client().remoteIP().toString());
  sendJsonResponse(1);
}

// Handle stop gate request
void handleStop() {
  activateRelay(RELAY_STOP, 1000);
  logGateAction("Stop", server.client().remoteIP().toString());
  sendJsonResponse(2);
}

// Handle stay open gate request
void handleStayOpen() {
  logGateAction("Stay Open", server.client().remoteIP().toString());
  if (digitalRead(GATE_OPEN) == HIGH) {
    activateRelay(RELAY_STOP, 1000);
    sendJsonResponse(10);
  } else {
    activateRelay(RELAY_OPEN, 1000);
    sendJsonResponse(4);

    unsigned long startTime = millis();
    while (digitalRead(GATE_OPEN) == LOW) {
      server.handleClient();
      if (millis() - startTime > 40000) break;
    }
    activateRelay(RELAY_STOP, 1000);
  }
}

// Handle set settings request
void handleSetSettings() {
  if (server.hasArg("plain")) {
    String json = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
      sendJsonResponse(5);
      return;
    }

    lightEnabled = doc["enableL"];
    lightDuration = doc["lightDuration"];
    lightOnTime = doc["lightOnTime"];
    lightOffTime = doc["lightOffTime"];
    strlcpy(gatePassword, doc["password"] | "", sizeof(gatePassword));
    strlcpy(gateBehavior, doc["behavior"] | "", sizeof(gateBehavior));

    saveSettings();
    logGateAction("Update Settings", server.client().remoteIP().toString());
    sendJsonResponse(6);
  } else {
    sendJsonResponse(7);
  }
}

// Handle get settings request
void handleGetSettings() {
  DynamicJsonDocument doc(1024);
  doc["code"] = 8;
  doc["settings"]["enableL"] = lightEnabled;
  doc["settings"]["lightDuration"] = lightDuration;
  doc["settings"]["lightOnTime"] = lightOnTime;
  doc["settings"]["lightOffTime"] = lightOffTime;
  doc["settings"]["password"] = gatePassword;
  doc["settings"]["behavior"] = gateBehavior;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Handle get status request
void handleGetStatus() {
  DynamicJsonDocument doc(1024);
  doc["code"] = 9;

  if (digitalRead(GATE_OPEN) == HIGH) {
    doc["gateStatus"] = 1;
  } else if (digitalRead(GATE_CLOSE) == HIGH) {
    doc["gateStatus"] = 2;
  } else {
    doc["gateStatus"] = 3;
  }

  doc["lightStatus"] = (digitalRead(RELAY_LIGHT) == HIGH) ? 1 : 2;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Handle get logs request
void handleGetLogs() {
  File logFile = SPIFFS.open(LOG_FILE, FILE_READ);
  if (!logFile) {
    sendJsonResponse(12);  // Error code for file not found
    return;
  }

  String logs = logFile.readString();
  logFile.close();

  DynamicJsonDocument doc(1024 + logs.length());
  doc["code"] = 11;
  doc["logs"] = logs;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);

  if (millis() > MAX_LOG_AGE) {
    resetLogs();
  }
}

// Handle get system info request
void handleGetSystemInfo() {
  DynamicJsonDocument doc(1024);

  unsigned long uptime = millis() / 1000;
  char uptimeStr[20];
  sprintf(uptimeStr, "%d days %02d:%02d:%02d", uptime / 86400, (uptime % 86400) / 3600, (uptime % 3600) / 60, uptime % 60);
  doc["uptime"] = uptimeStr;

  doc["usedMemory"] = SPIFFS.usedBytes();
  doc["freeMemory"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();

  File errorLogFile = SPIFFS.open("/error_log.txt", "r");
  if (errorLogFile) {
    doc["errorLogs"] = errorLogFile.readString();
    errorLogFile.close();
  } else {
    doc["errorLogs"] = "No error logs available";
  }

  doc["code"] = 12;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Utility functions
// -----------------

// Check if current time is within the light operation range
bool isTimeWithinRange() {
  int currentTime = getCurrentTime();
  if (lightOnTime < lightOffTime) {
    return currentTime >= lightOnTime && currentTime <= lightOffTime;
  } else {
    return currentTime >= lightOnTime || currentTime <= lightOffTime;
  }
}

// Get current time as minutes since midnight
int getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

// Handle keypad input
void handleKeypad() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    enteredCode += key;
    lastKeyPressTime = millis();
    if (enteredCode.equals(gatePassword)) {
      logGateAction("Gate open by key", WiFi.localIP().toString());
      performGateAction();
      enteredCode = "";
    } else if (enteredCode.length() >= strlen(gatePassword)) {
      enteredCode = enteredCode.substring(1);
    }
  }

  if (millis() - lastKeyPressTime > keyPressTimeout) {
    enteredCode = "";
  }
}

// Update gate LED status
void updateGateLedStatus() {
  if (digitalRead(GATE_CLOSE) == HIGH) {
    digitalWrite(GATE_CLOSE_LED, HIGH);
    digitalWrite(GATE_OPEN_LED, LOW);
  } else if (digitalRead(GATE_OPEN) == HIGH) {
    digitalWrite(GATE_OPEN_LED, HIGH);
    digitalWrite(GATE_CLOSE_LED, LOW);
  } else {
    digitalWrite(GATE_OPEN_LED, LOW);
    digitalWrite(GATE_CLOSE_LED, LOW);
  }
}

// Turn off the light
void turnOffLight() {
  digitalWrite(RELAY_LIGHT, LOW);
  lightTicker.detach();
}

// Perform gate action based on behavior setting
void performGateAction() {
  if (strcmp(gateBehavior, "open") == 0) {
    handleOpen();
  } else if (strcmp(gateBehavior, "stayopen") == 0) {
    handleStayOpen();
  }
}

// Activate a relay for a specified duration
void activateRelay(int pin, int duration) {
  digitalWrite(pin, HIGH);
  delay(duration);
  digitalWrite(pin, LOW);
}

// Log gate action
void logGateAction(const char* action, String ipAddress) {
  struct tm timeinfo;
  char dateTime[64] = "Time not set";
  
  if (timeInitialized && getLocalTime(&timeinfo)) {
    strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }

  File logFile = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if (!logFile) {
    logError("Failed to open log file for appending");
    return;
  }

  logFile.printf("%s | IP: %s | %s\n", dateTime, ipAddress.c_str(), action);
  logFile.close();
}

// Log error message
void logError(const char* errorMessage) {
  File errorLogFile = SPIFFS.open("/error_log.txt", "a");
  if (errorLogFile) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
      errorLogFile.printf("[%s] %s\n", timeStr, errorMessage);
    } else {
      errorLogFile.printf("%s\n", errorMessage);
    }
    errorLogFile.close();
  }
}

// Reset logs
void resetLogs() {
  File logFile = SPIFFS.open(LOG_FILE, FILE_WRITE);
  if (!logFile) {
    logError("Failed to open log file for resetting");
    return;
  }
  logFile.println("Logs reset");
  logFile.close();
}

// Handle file read for web server
bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

// Get content type for web server
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

// Send JSON response
void sendJsonResponse(int code) {
  String response = "{\"code\":" + String(code) + "}";
  server.send(200, "application/json", response);
}