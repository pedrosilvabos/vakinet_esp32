#include "base_lora.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_FREQUENCY 915E6
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 125E3
#define LORA_CODING_RATE 5

void LoRaBase::begin() {
  Serial.begin(115200);
  Serial.println("LoRa Base setup started");
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x70, 0x47, 0xAC});
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x65, 0x76, 0x48});
  nodeMacs.push_back({0x08, 0xA6, 0xF7, 0x0C, 0x04, 0x1C});

  setupWiFi();
  setupLoRa();

  jsonDoc.clear();
  JsonArray dataArray = jsonDoc.to<JsonArray>();
}

void LoRaBase::setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Error initializing LoRa");
    while (1);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.onReceive(LoRaBase::onReceiveLoRa);
  LoRa.receive();

  Serial.println("LoRa initialized");
}

void LoRaBase::sendTriggerToNode(const uint8_t* nodeId) {
  uint8_t msg[7] = {1}; // Command byte (1) followed by 6-byte MAC
  memcpy(msg + 1, nodeId, 6);
  LoRa.beginPacket();
  LoRa.write(msg, sizeof(msg));
  LoRa.endPacket();
  LoRa.receive();
  Serial.printf("Sent trigger to node %02X:%02X:%02X:%02X:%02X:%02X\n",
                nodeId[0], nodeId[1], nodeId[2], nodeId[3], nodeId[4], nodeId[5]);
}

void LoRaBase::update() {
  static unsigned long lastTriggerTime = 0;
  static size_t currentNodeIndex = 0;
  static bool waitingForResponse = false;
  static unsigned long triggerSentTime = 0;
  const unsigned long triggerInterval = 5000;
  const unsigned long responseTimeout = 500;

  unsigned long now = millis();

  if (!waitingForResponse && now - lastTriggerTime >= triggerInterval) {
    if (nodeMacs.empty()) return;

    sendTriggerToNode(nodeMacs[currentNodeIndex].data());
    waitingForResponse = true;
    triggerSentTime = now;

    currentNodeIndex = (currentNodeIndex + 1) % nodeMacs.size();
    if (currentNodeIndex == 0) {
      lastTriggerTime = now;
    }
  }

  if (waitingForResponse && now - triggerSentTime >= responseTimeout) {
    Serial.printf("Timeout waiting for response from node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][0],
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][1],
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][2],
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][3],
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][4],
                  nodeMacs[currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1][5]);
    waitingForResponse = false;
  }

  processHttpQueue();
}

void LoRaBase::onReceiveLoRa(int packetSize) {
  if (packetSize == 0) return;

  char buffer[256];
  int len = 0;
  while (LoRa.available() && len < sizeof(buffer) - 1) {
    buffer[len++] = (char)LoRa.read();
  }
  buffer[len] = '\0';

  Serial.printf("Received LoRa: %s\n", buffer);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  if (!error) {
    int value = doc["value"];
    const char* deviceId = doc["deviceId"];

    jsonDoc.clear();
    JsonArray dataArray = jsonDoc.to<JsonArray>();
    JsonObject entry = dataArray.createNestedObject();
    entry["value"] = value;
    entry["deviceId"] = String(deviceId);

    lastReceivedData = "";
    serializeJson(jsonDoc, lastReceivedData);

    Message msg;
    msg.json = lastReceivedData;
    msg.receivedAt = millis();
    messageQueue.push(msg);

    Serial.printf("Response: %s\n", lastReceivedData.c_str());
  } else {
    Serial.println("Failed to parse incoming JSON");
  }
}