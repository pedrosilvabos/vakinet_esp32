#include "node_lora.h"
#include <WiFi.h>

#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_FREQUENCY 915E6
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 125E3
#define LORA_CODING_RATE 5

void LoRaNode::begin() {
  Serial.begin(115200);
  Serial.println("LoRa Node setup started");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setupLoRa();
}

void LoRaNode::setupLoRa() {
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.disconnect();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("Error initializing LoRa");
    while (1);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.onReceive(LoRaNode::onReceiveLoRa);
  LoRa.receive();

  Serial.println("LoRa initialized");
}


void LoRaNode::update() {
  updateBlink();
}

void LoRaNode::onReceiveLoRa(int packetSize) {
  if (packetSize == 0) return;

  uint8_t buffer[256];
  int len = 0;
  while (LoRa.available() && len < sizeof(buffer)) {
    buffer[len++] = LoRa.read();
  }

  if (len == 7 && buffer[0] == 1) {
    uint8_t nodeMac[6];
    esp_read_mac(nodeMac, ESP_MAC_WIFI_STA);
    if (memcmp(buffer + 1, nodeMac, 6) == 0) {
      Serial.println("Trigger Activated");
      blinking = true;
      blinkStartTime = millis();
      digitalWrite(LED_PIN, HIGH);
     
      
       int randomValue = random(0, 100);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char message[64];
  snprintf(message, sizeof(message), "{\"value\":%d,\"deviceId\":\"%s\"}", randomValue, macStr);

  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
  LoRa.receive();
  Serial.printf("LoRa sent: %s\n", message);
    }
  }
}