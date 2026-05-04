#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

//Firmware version
#define CURRENT_VERSION "0.0.2"

//Initialize Wifi client
WiFiClient client;
//Initialize board preferences
Preferences preferences;
//Initialize deviceId as empty string
String deviceId;

//Secrets go here

//Is OTA update happening?
bool otaInProgress = false;

//Used for onboard LED control
unsigned long lastBlink = 0;

//State variable for LED blinking
bool ledState = false;

//Generate a random, unique id for hardware nodes.
String generateId() {
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  char id[33];
  sprintf(id, "%08lx%08lx", r1, r2);
  return String(id);
}

//Setup function to connect to WiFi, check if device has Id in memory, if not it'll create a new id.
void setup() {
  //Begin on 115200 baud rate serial monitor
  Serial.begin(115200);
  delay(2000);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  //Connect to WiFi

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

//Check if device has existing Id in memory
  preferences.begin("device", false);
  deviceId = preferences.getString("id", "");
  if (deviceId == "") {
    Serial.println("No id found. Generating");
    // No id in memory, generate new one.
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
  // Flash LED if OTA in progress
  if (otaInProgress) {
    unsigned long now = millis();
    if (now - lastBlink > 500) {
      ledState = !ledState;
      digitalWrite(2, ledState ? HIGH : LOW);
      lastBlink = now;
    }
  }

  // Regularly check for updates every 10s if not updating
  static unsigned long lastCheck = 0;
  if (!otaInProgress && millis() - lastCheck > 10000) {
    lastCheck = millis();
    checkForUpdate();
  }
}

void checkForUpdate() {
  //Make HTTP request (POST) to check if host server has firmware updates
  if (WiFi.status() != WL_CONNECTED) return;

  //Create HTTPClient instance
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(api_key));

  //Create payload for POST request to host server

  String payload = "{\"device_id\":\"" + deviceId + "\",\"firmware_version\":\"" + CURRENT_VERSION + "\"}";
  int code = http.POST(payload);

  if (code > 0) {
    //Deserialize json response and check if there's an update available.
    String response = http.getString();
    Serial.println(response);

    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, response)) {
      const char* latestVersion = doc["latest_version"];
      if (doc.containsKey("latest_firmware_url")) {
        const char* firmwareUrl = doc["latest_firmware_url"];
        if (String(latestVersion) != String(CURRENT_VERSION)) {
          Serial.println("Update available!");
          //If host server has a binary file with updated firmware, we'll update the device.
          performOTA(String(firmwareUrl));
        } else {
          Serial.println("Firmware up to date.");
          digitalWrite(2, HIGH); // solid LED
        }
      }
    }
  } else {
    Serial.println("Request failed");
    digitalWrite(2, LOW); // error
  }

  http.end();
}
//Function to execute OTA updates to firmware.
void performOTA(String firmwareUrl) {
  otaInProgress = true;
  Serial.println("Starting OTA update...");

  //Obtains the .bin file on the host server containing firmware and updates the device's code.
  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed. Error (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      digitalWrite(2, LOW); // LED off on failure
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available.");
      digitalWrite(2, HIGH); // solid LED
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Update successful! Rebooting...");
      break;
  }

  otaInProgress = false; // Stop flashing after OTA
}
