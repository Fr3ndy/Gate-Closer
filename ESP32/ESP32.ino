#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include <Keypad.h>
#include <Ticker.h>
#include <SPIFFS.h>

const char* ssid = "****";
const char* password = "****";

// NTP server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

WebServer server(80);

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
bool lightEnabled = false;
int lightOnTime = 0;
int lightOffTime = 0;
int lightDuration = 10;
char gatePassword[20] = "";
char gateBehavior[10] = "";

unsigned long lastTimeSyncMillis = 0;
const unsigned long timeSyncInterval = 3600000; // 1 hour

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

unsigned long lastKeyPressTime = 0;
const unsigned long keyPressTimeout = 5000;

Ticker lightTicker;

// Function prototypes
void loadSettings();
void handleRoot();
void handleOpen();
void handleStop();
void handleStayOpen();
void handleSetSettings();
void handleGetSettings();
void handleGetStatus();
void syncTimeIfNeeded();
void handleKeypad();
bool isTimeWithinRange();
int getCurrentTime();
void saveSettings();
void turnOffLight();
void performGateAction();
void sendJsonResponse(int code);
void activateRelay(int pin, int duration);
void updateGateLedStatus();
void resetLogs();
void logGateAction(const char* action, String ipAddress);
void handleGetLogs();
bool handleFileRead(String path);
String getContentType(String filename);
void handleGetSystemInfo();
void logError(const char* errorMessage);

void setup() {
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    return;
  }

  // Set pin modes
  pinMode(RELAY_OPEN, OUTPUT);
  pinMode(RELAY_STOP, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(GATE_OPEN, INPUT);
  pinMode(GATE_OPEN_LED, OUTPUT);
  pinMode(GATE_CLOSE, INPUT);
  pinMode(GATE_CLOSE_LED, OUTPUT);
  pinMode(CONNECTION_LED, OUTPUT);

  // Initialize relays and LED
  digitalWrite(RELAY_OPEN, LOW);
  digitalWrite(RELAY_STOP, LOW);
  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(CONNECTION_LED, LOW);

// Configura i pin delle colonne come input con pull-up
  for (byte i = 0; i < COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }

  // Configura i pin delle righe come output
  for (byte i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);  // Imposta inizialmente a HIGH
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(CONNECTION_LED, !digitalRead(CONNECTION_LED));
    delay(500);
  }
  digitalWrite(CONNECTION_LED, HIGH);

  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Load settings from EEPROM
  EEPROM.begin(512);
  loadSettings();

  // Set up server routes
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

}

void loop() {
  server.handleClient();
  syncTimeIfNeeded();
  handleKeypad();
  updateGateLedStatus();
}

void handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(404, "text/plain", "File Not Found");
  }
}

void handleOpen() {
  activateRelay(RELAY_OPEN, 1000);

  if (lightEnabled && isTimeWithinRange()) {
    digitalWrite(RELAY_LIGHT, HIGH);
    lightTicker.attach_ms(lightDuration * 1000, turnOffLight);
  }

  logGateAction("Open", server.client().remoteIP().toString());
  sendJsonResponse(1);
}

void turnOffLight() {
  digitalWrite(RELAY_LIGHT, LOW);
  lightTicker.detach();
}

void handleStop() {
  activateRelay(RELAY_STOP, 1000);
  logGateAction("Stop", server.client().remoteIP().toString());
  sendJsonResponse(2);
}

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

bool isTimeWithinRange() {
  int currentTime = getCurrentTime();
  if (lightOnTime < lightOffTime) {
    return currentTime >= lightOnTime && currentTime <= lightOffTime;
  } else {
    return currentTime >= lightOnTime || currentTime <= lightOffTime;
  }
}

int getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

void saveSettings() {
  EEPROM.put(0, lightEnabled);
  EEPROM.put(1, lightDuration);
  EEPROM.put(5, lightOnTime);
  EEPROM.put(9, lightOffTime);
  EEPROM.put(13, gatePassword);
  EEPROM.put(33, gateBehavior);
  EEPROM.commit();
}

void loadSettings() {
  EEPROM.get(0, lightEnabled);
  EEPROM.get(1, lightDuration);
  EEPROM.get(5, lightOnTime);
  EEPROM.get(9, lightOffTime);
  EEPROM.get(13, gatePassword);
  EEPROM.get(33, gateBehavior);
}

void syncTimeIfNeeded() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastTimeSyncMillis >= timeSyncInterval) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    lastTimeSyncMillis = currentMillis;
  }
}

void handleKeypad() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    enteredCode += key;
    lastKeyPressTime = millis();
    if (enteredCode.equals(gatePassword)) {
      logGateAction("Gate open by key", server.client().remoteIP().toString());
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

void performGateAction() {
  if (strcmp(gateBehavior, "open") == 0) {
    handleOpen();
  } else if (strcmp(gateBehavior, "stayopen") == 0) {
    handleStayOpen();
  }
}

void sendJsonResponse(int code) {
  String response = "{\"code\":" + String(code) + "}";
  server.send(200, "application/json", response);
}

void activateRelay(int pin, int duration) {
  digitalWrite(pin, HIGH);
  delay(duration);
  digitalWrite(pin, LOW);
}

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

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

void logGateAction(const char* action, String ipAddress) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    logError("Failed to obtain time");
    return;
  }

  char dateTime[64];
  strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &timeinfo);

  File logFile = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if (!logFile) {
    logError("Failed to open log file for appending");
    return;
  }

  logFile.printf("%s | IP: %s | %s\n", dateTime, ipAddress.c_str(), action);
  logFile.close();
}

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

  // Check if it's time to reset the logs
  if (millis() > MAX_LOG_AGE) {
    resetLogs();
  }
}

void resetLogs() {
  File logFile = SPIFFS.open(LOG_FILE, FILE_WRITE);
  if (!logFile) {
    logError("Failed to open log file for resetting");
    return;
  }
  logFile.println("Logs reset");
  logFile.close();
}

void handleGetSystemInfo() {
  DynamicJsonDocument doc(1024);

  // Uptime
  unsigned long uptime = millis() / 1000;  // Convert to seconds
  char uptimeStr[20];
  sprintf(uptimeStr, "%d days %02d:%02d:%02d", uptime / 86400, (uptime % 86400) / 3600, (uptime % 3600) / 60, uptime % 60);
  doc["uptime"] = uptimeStr;

  // Memory
  doc["usedMemory"] = SPIFFS.usedBytes();
  doc["freeMemory"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();

  // Error logs
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