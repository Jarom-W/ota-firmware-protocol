//This firmware is intended to be used on ESP32 LoRa v3 microcontroller boards from Heltec Automation.

//This firmware is for the gateway node of the LoRaWANN protocol. It's meant to be within WiFi and administer firmware to the other nodes outside of WiFi.
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

#define CURRENT_VERSION "0.0.2"
#define DEVICE_TYPE "lora_gateway"

WiFiClient client;
Preferences preferences;

String deviceId;

//Secrets

bool otaInProgress = false;

//LED State Enum

enum LedState {
  OFF,
  ON,
  UPDATE,
  ERROR
};

LedState ledState = OFF;

/* blink state tracking */
unsigned long lastBlinkFast = 0;
unsigned long lastBlinkSlow = 0;

bool blinkFastState = false;
bool blinkSlowState = false;

//LED control helper functions

void setLedState(LedState newState) {
  ledState = newState;

  // reset all blink memory when switching modes
  lastBlinkFast = 0;
  lastBlinkSlow = 0;
  blinkFastState = false;
  blinkSlowState = false;
}

//Blink LED functions

void blinkFast() {
  unsigned long now = millis();

  if (now - lastBlinkFast >= 100) {
    lastBlinkFast = now;
    blinkFastState = !blinkFastState;

    // active LOW LED
    digitalWrite(35, blinkFastState ? LOW : HIGH);
  }
}

void blinkSlow() {
  unsigned long now = millis();

  if (now - lastBlinkSlow >= 500) {
    lastBlinkSlow = now;
    blinkSlowState = !blinkSlowState;

    // active LOW LED
    digitalWrite(35, blinkSlowState ? LOW : HIGH);
  }
}

//LED Update Loop

void updateLed() {
  switch (ledState) {

    case ON:
      digitalWrite(35, LOW);   // LED ON (active LOW)
      break;

    case OFF:
      digitalWrite(35, HIGH);  // LED OFF
      break;

    case ERROR:
      blinkSlow();
      break;

    case UPDATE:
      blinkFast();
      break;
  }
}

//Generate unique ID for device to store in its memory

String generateId() {
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();

  char id[33];
  sprintf(id, "%08lx%08lx", r1, r2);

  return String(id);
}

//Setup function to run upon initialization

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(35, OUTPUT);
  digitalWrite(35, HIGH); // default OFF
  //Attempt to connect to wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  preferences.begin("device", false);

  //Check if board has a device ID in memory

  deviceId = preferences.getString("id", "");

  if (deviceId == "") {
    Serial.println("No id found. Generating...");
    deviceId = generateId();
    preferences.putString("id", deviceId);
  } else {
    Serial.println("Loaded existing id");
  }

  Serial.println("Device ID:");
  Serial.println(deviceId);

  preferences.end();
}

void loop() {
  updateLed();
  //checks for own firmware update.
  //If update, update then continue.
  //Recognize all device ids in field.
  //Check if each type has an update.
  //If update, send out burst to prepare for OTA.
  //After a few bursts, switch to waiting and receiving mode.
  //Send firmware update in chunks. Wait for reception from all nodes.
  //If no update, just go into telemetry listening mode.
  //Post telemetry to server.


  static unsigned long lastCheck = 0;

  if (!otaInProgress && millis() - lastCheck > 10000) {
    lastCheck = millis();
    checkForUpdate();
  }
}

void checkForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(api_key));

  String payload =
    "{\"device_id\":\"" + deviceId +
    "\",\"firmware_version\":\"" + CURRENT_VERSION +
    "\",\"device_type\":\"" + DEVICE_TYPE + "\"}";

  int code = http.POST(payload);

  if (code > 0) {
    String response = http.getString();
    Serial.println(response);

    StaticJsonDocument<256> doc;

    if (!deserializeJson(doc, response)) {

      const char* latestVersion = doc["latest_version"];

      if (doc.containsKey("latest_firmware_url")) {

        const char* firmwareUrl = doc["latest_firmware_url"];

        if (String(latestVersion) != String(CURRENT_VERSION)) {

          Serial.println("Update available!");
          performOTA(String(firmwareUrl));

        } else {

          Serial.println("Firmware up to date.");
          setLedState(ON);
        }
      }
    }
  } else {
    Serial.println("Request failed");
    setLedState(ERROR);
  }

  http.end();
}

//Perform the OTA update over WiFi (Update the gateway, not the LoRaWANN nodes)

void performOTA(String firmwareUrl) {
  otaInProgress = true;

  setLedState(UPDATE);

  Serial.println("Starting OTA update...");

  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {

    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed. Error (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());

      setLedState(ERROR);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available.");
      setLedState(ON);
      break;

    case HTTP_UPDATE_OK:
      Serial.println("Update successful! Rebooting...");
      break;
  }

  otaInProgress = false;
}
