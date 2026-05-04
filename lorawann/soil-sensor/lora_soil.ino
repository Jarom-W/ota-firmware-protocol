//This code is meant for ESP32 LoRa v3 microcontroller boards from Heltec Automation

#include <Preferences.h>
#include "LoRaWan_APP.h"

//Define all the constants used throughout
#define CURRENT_VERSION "0.0.1"
#define DEVICE_TYPE "lora_soil" //Prototype soil sensor node

#define RF_FREQUENCY 915000000 //915MHz frequency for LoRa


//Radio wave physics config
#define LORA_SPREADING_FACTOR 9
#define TX_OUTPUT_POWER 17
#define LORA_CODINGRATE 1
#define LORA_BANDWIDTH 0

#define BUFFER_SIZE 120

//Packet types
#define PKT_TELEMETRY   0x01
#define PKT_HEARTBEAT   0x02
#define PKT_COMMAND     0x03
#define PKT_READY       0x04
#define PKT_OTA_CHUNK   0x05
#define PKT_OTA_ACK     0x06

//Enum for states the device can be in.
enum NodeState {
  STATE_NORMAL,
  STATE_LISTEN,
  STATE_PREPARE_OTA,
  STATE_OTA
};

NodeState currentState = STATE_NORMAL;

//Device Id set up
Preferences preferences;
String deviceIdStr;
uint32_t deviceId;

//Radio states
static RadioEvents_t RadioEvents;

bool txDone = true;

uint8_t rxBuffer[BUFFER_SIZE];
uint16_t rxSize = 0;

//Timing variables
unsigned long lastHeartbeat = 0;
unsigned long lastTelemetry = 0;

const unsigned long HEARTBEAT_INTERVAL = 30000;
const unsigned long TELEMETRY_INTERVAL = 5000;
//Global vars
uint16_t msgCounter = 0;

//Packet structure
struct __attribute__((packed)) HeartbeatV1 {
  uint8_t  packet_type;
  uint16_t msg_id;

  uint32_t device_id;
  uint8_t  device_type;

  uint8_t  fw_major;
  uint8_t  fw_minor;
  uint8_t  fw_patch;

  uint8_t  protocol_ver;

  uint8_t  battery;
  uint8_t  status_flags;

  uint16_t uptime_sec;
};

struct __attribute__((packed)) CommandPacket {
  uint8_t  packet_type;
  uint16_t msg_id;

  uint32_t target_version;
  uint32_t firmware_size;
  uint8_t  flags;
};

//Generate new device id function
String generateId() {
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  char id[33];
  sprintf(id, "%08lx%08lx", r1, r2);
  return String(id);
}
//Helper function to convert hexidecimal to an unsigned 32 bit integer
uint32_t hexToUint32(String s) {
  return strtoul(s.c_str(), NULL, 16);
}

//Radio callback events
void OnTxDone(void) {
  Serial.println("TX done");
  txDone = true;

  // always return to RX mode
  Radio.Rx(0);
}

void OnTxTimeout(void) {
  Serial.println("TX timeout");
  txDone = true;
  Radio.Rx(0);
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  memcpy(rxBuffer, payload, size);
  rxSize = size;

  uint8_t type = rxBuffer[0];

  Serial.print("📥 RX packet type: ");
  Serial.println(type);

  if (type == PKT_COMMAND) {
    Serial.println("⚡ OTA command received");
    currentState = STATE_PREPARE_OTA;
  }

  Radio.Rx(0);
}

//Send 'heartbeat' or telemetry updates to gateway
void sendHeartbeat() {
  HeartbeatV1 hb;

  hb.packet_type = PKT_HEARTBEAT;
  hb.msg_id = msgCounter++;
  hb.device_id = deviceId;

  hb.device_type = DEVICE_TYPE;

  hb.fw_major = 0;
  hb.fw_minor = 0;
  hb.fw_patch = 1;

  hb.protocol_ver = 1;

  hb.battery = 100; // placeholder
  hb.status_flags = 0;

  hb.uptime_sec = millis() / 1000;

  Radio.Send((uint8_t*)&hb, sizeof(hb));
  Serial.println("📡 Heartbeat sent");
}

//Setup function
void setup() {
  Serial.begin(115200);
  delay(2000);

 //See if device has existing id in memory
  preferences.begin("device", false);

  deviceIdStr = preferences.getString("id", "");
  if (deviceIdStr == "") {
    //If no existing device id, generate a new one
    deviceIdStr = generateId();
    preferences.putString("id", deviceIdStr);
  }

  preferences.end();

  deviceId = hexToUint32(deviceIdStr);

  Serial.println("Device ID:");
  Serial.println(deviceIdStr);

//Initialize LoRa on the board
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);

  Radio.SetTxConfig(
    MODEM_LORA,
    TX_OUTPUT_POWER,
    0,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH,
    false,
    true,
    0,
    0,
    false,
    3000
  );

  Serial.println("LoRa initialized");

  // start listening immediately
  Radio.Rx(0);
}

void loop() {
  Radio.IrqProcess();

  unsigned long now = millis();

  //Send telemetry to gateway
  if (currentState == STATE_NORMAL && txDone &&
      now - lastHeartbeat > HEARTBEAT_INTERVAL) {

    lastHeartbeat = now;
    txDone = false;

    sendHeartbeat();
  }

  //Manage state actions
  if (currentState == STATE_PREPARE_OTA) {
    Serial.println("Preparing OTA state...");

    delay(random(500, 2000));

    Serial.println("Ready for OTA (placeholder)");
    currentState = STATE_NORMAL;
  }

  //If transmission finishes, go back to listening on receiver mode
  if (txDone) {
    Radio.Rx(0);
  }
}
