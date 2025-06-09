#include "base_espnow.h"
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

void EspNowBase::begin() {
  Serial.begin(115200);
  Serial.println("ESP-NOW Base setup started");
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x70, 0x47, 0xAC});
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x65, 0x76, 0x48});
  nodeMacs.push_back({0x08, 0xA6, 0xF7, 0x0C, 0x04, 0x1C});

  setupWiFi();
  setupEspNow();

  jsonDoc.clear();
  JsonArray dataArray = jsonDoc.to<JsonArray>();
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
  for (size_t i = 0; i < nodeMacs.size() && i < 20; i++) {
    memcpy(peerInfo.peer_addr, nodeMacs[i].data(), 6);
    if (!esp_now_is_peer_exist(nodeMacs[i].data())) {
      if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.printf("Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                      nodeMacs[i][0], nodeMacs[i][1], nodeMacs[i][2],
                      nodeMacs[i][3], nodeMacs[i][4], nodeMacs[i][5]);
      } else {
        Serial.printf("ESP-NOW peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      nodeMacs[i][0], nodeMacs[i][1], nodeMacs[i][2],
                      nodeMacs[i][3], nodeMacs[i][4], nodeMacs[i][5]);
      }
    }
  }

  uint8_t chan;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&chan, &second);
  Serial.printf("BASE Wi-Fi Channel: %d\n", chan);
}

void EspNowBase::addPeer(const uint8_t* mac_addr) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac_addr, 6);
  peerInfo.channel = 6;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(mac_addr)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
      Serial.printf("ESP-NOW peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    }
  }
}

void EspNowBase::removePeer(const uint8_t* mac_addr) {
  if (esp_now_is_peer_exist(mac_addr)) {
    if (esp_now_del_peer(mac_addr) != ESP_OK) {
      Serial.printf("Failed to remove peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
      Serial.printf("ESP-NOW peer removed: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac_addr[0], mac_addr[1], mac_addr[2],
                    mac_addr[3], mac_addr[4], mac_addr[5]);
    }
  }
}

void EspNowBase::sendTriggerToNode(const uint8_t* nodeId) {
  struct TriggerMessage { uint8_t command; } msg = {1};
  esp_err_t result = esp_now_send(nodeId, (uint8_t*)&msg, sizeof(msg));
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
  const unsigned long triggerInterval = 5000;
  const unsigned long responseTimeout = 500;

  unsigned long now = millis();

  if (!waitingForResponse && now - lastTriggerTime >= triggerInterval) {
    if (nodeMacs.empty()) return;

    if (currentNodeIndex > 0 && nodeMacs.size() > 20) {
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

void EspNowBase::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void EspNowBase::onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len) {
  char buffer[256];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';

  Serial.printf("Received ESP-NOW: %s\n", buffer);

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