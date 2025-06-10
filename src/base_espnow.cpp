#include "base_espnow.h"
#include <esp_wifi.h>

void EspNowBase::begin() {
  Serial.begin(115200);
  Serial.println("ESP-NOW Base setup started");

  nodeMacs = {
    {0x4C, 0x11, 0xAE, 0x70, 0x47, 0xAC},
    {0x4C, 0x11, 0xAE, 0x65, 0x76, 0x48},
    {0x08, 0xA6, 0xF7, 0x0C, 0x04, 0x1C}
  };

  setupWiFi();
  setupEspNow();

  jsonDoc.clear();
  jsonDoc.to<JsonArray>();
}

void EspNowBase::setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(EspNowBase::onDataSent);
  esp_now_register_recv_cb(EspNowBase::onReceiveEspNow);

  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = 6;
  peerInfo.encrypt = false;

  for (const auto& mac : nodeMacs) {
    addNodeMac(mac.data());
    if (!esp_now_is_peer_exist(mac.data())) {
      memcpy(peerInfo.peer_addr, mac.data(), 6);
      if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.printf("Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      } else {
        Serial.printf("ESP-NOW peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      }
    }
  }

  uint8_t chan;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&chan, &second);
  Serial.printf("BASE Wi-Fi Channel: %d\n", chan);
}

void EspNowBase::addPeer(const uint8_t* mac_addr) {
  if (mac_addr == nullptr) return;

  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = 6;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, mac_addr, 6);

  if (!esp_now_is_peer_exist(mac_addr)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
      Serial.printf("ESP-NOW peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
      addNodeMac(mac_addr);
    }
  }
}

void EspNowBase::removePeer(const uint8_t* mac_addr) {
  if (mac_addr == nullptr) return;

  if (esp_now_is_peer_exist(mac_addr)) {
    if (esp_now_del_peer(mac_addr) != ESP_OK) {
      Serial.printf("Failed to remove peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
      Serial.printf("ESP-NOW peer removed: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
      std::array<uint8_t, 6> macArray;
      memcpy(macArray.data(), mac_addr, 6);
      auto it = std::find(nodeMacs.begin(), nodeMacs.end(), macArray);
      if (it != nodeMacs.end()) {
        nodeMacs.erase(it);
      }
    }
  }
}

void EspNowBase::sendTriggerToNode(const uint8_t* nodeId) {
  if (nodeId == nullptr) return;

  struct TriggerMessage { uint8_t command; } msg = {1};
  esp_err_t result = esp_now_send(nodeId, reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
  if (result == ESP_OK) {
    Serial.printf("Sent trigger to node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  nodeId[0], nodeId[1], nodeId[2], nodeId[3], nodeId[4], nodeId[5]);
  } else {
    Serial.printf("Failed to send trigger to node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  nodeId[0], nodeId[1], nodeId[2], nodeId[3], nodeId[4], nodeId[5]);
  }
}

void EspNowBase::update() {
  static unsigned long lastTriggerTime = 0;
  static size_t currentNodeIndex = 0;
  static bool waitingForResponse = false;
  static unsigned long triggerSentTime = 0;
  constexpr unsigned long triggerInterval = 5000;
  constexpr unsigned long responseTimeout = 500;

  unsigned long now = millis();

  if (!waitingForResponse && now - lastTriggerTime >= triggerInterval) {
    if (nodeMacs.empty()) return;

    if (currentNodeIndex > 0 && nodeMacs.size() > MAX_NODES) {
      removePeer(nodeMacs[currentNodeIndex - 1].data());
    }

    addPeer(nodeMacs[currentNodeIndex].data());
    sendTriggerToNode(nodeMacs[currentNodeIndex].data());
    waitingForResponse = true;
    triggerSentTime = now;

    currentNodeIndex = (currentNodeIndex + 1) % nodeMacs.size();
    if (currentNodeIndex == 0) {
      lastTriggerTime = now;
    }
  }

  if (waitingForResponse && now - triggerSentTime >= responseTimeout) {
    const size_t index = currentNodeIndex ? currentNodeIndex - 1 : nodeMacs.size() - 1;
    Serial.printf("Timeout waiting for response from node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  nodeMacs[index][0], nodeMacs[index][1], nodeMacs[index][2],
                  nodeMacs[index][3], nodeMacs[index][4], nodeMacs[index][5]);
    waitingForResponse = false;
  }

  processMessageQueue();
  processHttpQueue();
  checkHeap();
}

void EspNowBase::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void EspNowBase::onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len) {
  if (!incomingData || len <= 0 || len > 255) return;

  char buffer[256];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';

  Serial.printf("Received ESP-NOW: %s\n", buffer);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  if (error) {
    Serial.println("Failed to parse incoming JSON");
    return;
  }

  int value = doc["value"] | -1;
  const char* deviceId = doc["deviceId"] | "";

  jsonDoc.clear();
  JsonArray dataArray = jsonDoc.to<JsonArray>();
  JsonObject entry = dataArray.createNestedObject();
  entry["value"] = value;
  entry["deviceId"] = deviceId;

  if (serializeJson(jsonDoc, lastReceivedData, sizeof(lastReceivedData)) == 0) {
    Serial.println("Failed to serialize JSON to lastReceivedData");
    strcpy(lastReceivedData, "[]");
    return;
  }

  Base::enqueueMessage(lastReceivedData); // Call static method
  Serial.printf("Response: %s\n", lastReceivedData);
}