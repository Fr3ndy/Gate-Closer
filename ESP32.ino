#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <Ticker.h>

// Mappa dei codici di risposta del server:
// {"code":1} - Operazione di apertura completata con successo
// {"code":2} - Operazione di stop completata con successo
// {"code":3} - Cancello in movimento
// {"code":4} - Operazione di apertura fissa completata con successo
// {"code":5} - Errore di deserializzazione durante l'impostazione
// {"code":6} - Impostazioni salvate con successo
// {"code":7} - Nessun dato inviato per l'impostazione
// {"code":8} - Impostazioni restituite con successo
// {"code":9} - Stato del cancello restituito con successo

extern const char* html;

const char* ssid = "****";
const char* password = "****";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

WebServer server(80);

#define RELAY_OPEN 12
#define RELAY_STOP 33
#define RELAY_LIGHT 14

#define GATE_OPEN 35
#define GATE_OPEN_LED 26
#define GATE_CLOSE 34
#define GATE_CLOSE_LED 25

bool lightEnabled = false;
int lightOnTime = 0;
int lightOffTime = 0;
int lightDuration = 10;
char gatePassword[20] = "";
char gateBehavior[10] = "";

unsigned long lastTimeSyncMillis = 0;
const unsigned long timeSyncInterval = 3600000;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 4, 16, 17, 5 };
byte colPins[COLS] = { 18, 19, 21, 22 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
String enteredCode = "";

unsigned long lastKeyPressTime = 0;
const unsigned long keyPressTimeout = 5000;

Ticker lightTicker;

// Dichiarazione di tutte le funzioni
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

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_OPEN, OUTPUT);
  pinMode(RELAY_STOP, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(GATE_OPEN, INPUT);
  pinMode(GATE_OPEN_LED, OUTPUT);
  pinMode(GATE_CLOSE, INPUT);
  pinMode(GATE_CLOSE_LED, OUTPUT);

  digitalWrite(RELAY_OPEN, LOW);
  digitalWrite(RELAY_STOP, LOW);
  digitalWrite(RELAY_LIGHT, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connessione al WiFi...");
  }
  Serial.println("Connesso al WiFi");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  EEPROM.begin(512);
  loadSettings();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/open", HTTP_GET, handleOpen);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/stayopen", HTTP_GET, handleStayOpen);
  server.on("/setsettings", HTTP_POST, handleSetSettings);
  server.on("/getsettings", HTTP_GET, handleGetSettings);
  server.on("/getstatus", HTTP_GET, handleGetStatus);

  server.begin();
}

void loop() {
  server.handleClient();
  syncTimeIfNeeded();
  handleKeypad();

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

void handleRoot() {
  server.send(200, "text/html", html);
}

void handleOpen() {
  digitalWrite(RELAY_OPEN, HIGH);
  delay(1000);
  digitalWrite(RELAY_OPEN, LOW);

  if (lightEnabled && isTimeWithinRange()) {
    digitalWrite(RELAY_LIGHT, HIGH);
    lightTicker.attach_ms(lightDuration * 1000, turnOffLight);
  }
  server.send(200, "application/json", "{\"code\":1}");
}

void turnOffLight() {
  digitalWrite(RELAY_LIGHT, LOW);
  lightTicker.detach();  // Stop the ticker
}

void handleStop() {
  digitalWrite(RELAY_STOP, HIGH);
  delay(1000);
  digitalWrite(RELAY_STOP, LOW);
  server.send(200, "application/json", "{\"code\":2}");
}

void handleStayOpen() {
  if (digitalRead(GATE_OPEN) == HIGH) {
    digitalWrite(RELAY_STOP, HIGH);
    delay(1000);
    digitalWrite(RELAY_STOP, LOW);
  } else {
    digitalWrite(RELAY_OPEN, HIGH);
    delay(1000);
    digitalWrite(RELAY_OPEN, LOW);
    while (digitalRead(GATE_OPEN) == LOW) {
      delay(10);
    }
  }
  server.send(200, "application/json", "{\"code\":4}");
}

void handleSetSettings() {
  if (server.hasArg("plain")) {
    String json = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
      server.send(400, "application/json", "{\"code\":5}");
      return;
    }

    lightEnabled = doc["enableL"];
    lightDuration = doc["lightDuration"];
    lightOnTime = doc["lightOnTime"];
    lightOffTime = doc["lightOffTime"];
    strlcpy(gatePassword, doc["password"] | "", sizeof(gatePassword));
    strlcpy(gateBehavior, doc["behavior"] | "", sizeof(gateBehavior));

    saveSettings();

    server.send(200, "application/json", "{\"code\":6}");
  } else {
    server.send(400, "application/json", "{\"code\":7}");
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
    doc["gateStatus"] = 1; // Aperto fisso
  } else if (digitalRead(GATE_CLOSE) == HIGH) {
    doc["gateStatus"] = 2; // Chiuso
  } else {
    doc["gateStatus"] = 3; // In movimento
  }

  doc["lightStatus"] = (digitalRead(RELAY_LIGHT) == HIGH) ? 1 : 2; // 1: Accesa, 2: Spenta

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

    Serial.println(enteredCode);
    if (enteredCode.equals(gatePassword)) {
      Serial.println("Password corretta");

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
  } else {
    Serial.println("Comportamento non riconosciuto");
  }
}