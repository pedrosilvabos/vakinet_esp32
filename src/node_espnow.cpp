#include "node_espnow.h"
#include <esp_wifi.h>

void EspNowNode::begin() {
  Serial.begin(115200);
  Serial.println("ESP-NOW Node setup started");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  setupWiFi();
  setupEspNow();
}

void EspNowNode::setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("BASE not found. Defaulting to channel 6");
  String mac = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.println(mac);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

  uint8_t chan;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&chan, &second);
  Serial.printf("NODE Wi-Fi Channel: %d\n", chan);
}

void EspNowNode::setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(EspNowNode::onDataSent);
  esp_now_register_recv_cb(EspNowNode::onReceiveEspNow);

  esp_now_peer_info_t peerInfo = {};
  uint8_t baseMac[] = {0xBC, 0xDD, 0xC2, 0xCE, 0xCA, 0x58}; // Update with actual base MAC
  memcpy(peerInfo.peer_addr, baseMac, 6);
  peerInfo.channel = 6;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(baseMac)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    } else {
      Serial.println("ESP-NOW peer added");
    }
  }
}



void EspNowNode::update() {
  updateBlink();
}

void EspNowNode::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void EspNowNode::onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len) {
  struct TriggerMessage { uint8_t command; };
  if (len == sizeof(TriggerMessage)) {
    TriggerMessage msg;
    memcpy(&msg, incomingData, sizeof(TriggerMessage));
    if (msg.command == 1) {
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

  uint8_t baseMac[] = {0xBC, 0xDD, 0xC2, 0xCE, 0xCA, 0x58}; // Update with actual base MAC
  esp_err_t result = esp_now_send(baseMac, (uint8_t *)message, strlen(message));
  if (result == ESP_OK) {
    Serial.printf("ESP-NOW sent: %s\n", message);
  } else {
    Serial.println("ESP-NOW send failed");
  }
    }
  } else {
    Serial.println("Received data is not a valid trigger message");
  }
}