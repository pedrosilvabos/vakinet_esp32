#include "base.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Initialize static members
std::vector<std::array<uint8_t, 6>> Base::nodeMacs = {};
String Base::lastReceivedData = "[]";
StaticJsonDocument<1024> Base::jsonDoc;
std::queue<Base::EspNowMessage> Base::messageQueue;

void Base::begin() {
  Serial.println("Base setup started");
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x70, 0x47, 0xAC}); // Node MAC address
  nodeMacs.push_back({0x4C, 0x11, 0xAE, 0x65, 0x76, 0x48}); // Node MAC address
  nodeMacs.push_back({0x08, 0xA6, 0xF7, 0x0C, 0x04, 0x1C}); // Node MAC address


  // Add more MAC addresses here (up to 200)
  setupWiFi();
  setupEspNow();

  jsonDoc.clear();
  JsonArray dataArray = jsonDoc.to<JsonArray>();
}

void Base::setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(Base::onDataSent);
  esp_now_register_recv_cb(Base::onReceiveEspNow);

  // Add up to 14 peers (ESP-NOW unencrypted peer limit)
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = 6;
  peerInfo.encrypt = false;
  for (size_t i = 0; i < nodeMacs.size() && i < 14; i++) {
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
}

void Base::addPeer(const uint8_t* mac_addr) {
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

void Base::removePeer(const uint8_t* mac_addr) {
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

void Base::update() {
  static unsigned long lastTriggerTime = 0;
  static size_t currentNodeIndex = 0;
  static bool waitingForResponse = false;
  static unsigned long triggerSentTime = 0;
  const unsigned long triggerInterval = 5000;  // Time between starting a new cycle
  const unsigned long responseTimeout = 500;   // Timeout for node response (ms)

  unsigned long now = millis();

  // Send trigger to next node if not waiting for a response
  if (!waitingForResponse && now - lastTriggerTime >= triggerInterval) {
    if (nodeMacs.empty()) return;

    // Remove previous peer if necessary (to manage peer limit)
    if (currentNodeIndex > 0 && nodeMacs.size() > 14) {
      removePeer(nodeMacs[currentNodeIndex - 1].data());
    }

    // Add current node as peer and send trigger
    if (currentNodeIndex < nodeMacs.size()) {
      addPeer(nodeMacs[currentNodeIndex].data());
      sendTriggerToNode(nodeMacs[currentNodeIndex].data());
      waitingForResponse = true;
      triggerSentTime = now;
    }

    // Move to next node or reset to start
    currentNodeIndex = (currentNodeIndex + 1) % nodeMacs.size();
    if (currentNodeIndex == 0) {
      lastTriggerTime = now; // Reset cycle timer after full cycle
    }
  }

  // Check for timeout
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

  // Process HTTP queue periodically
  processHttpQueue();
}

void Base::sendTriggerToNode(const uint8_t* mac_addr) {
  struct TriggerMessage msg = {1};
  esp_err_t result = esp_now_send(mac_addr, (uint8_t*)&msg, sizeof(msg));
  if (result == ESP_OK) {
    Serial.printf("Sent trigger to node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_addr[0], mac_addr[1], mac_addr[2], 
                  mac_addr[3], mac_addr[4], mac_addr[5]);
  } else {
    Serial.printf("Failed to send trigger to node %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_addr[0], mac_addr[1], mac_addr[2], 
                  mac_addr[3], mac_addr[4], mac_addr[5]);
  }
}

void Base::sendHttp() {
  const char root_ca[] = 
   "-----BEGIN CERTIFICATE-----\n"
  "MIIDpTCCA0ugAwIBAgIQfAwp5EhN0tkNy410DD/sjzAKBggqhkjOPQQDAjA7MQsw\n"
  "CQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMQwwCgYD\n"
  "VQQDEwNXRTEwHhcNMjUwNjA2MTgwMjA4WhcNMjUwOTA0MTkwMjAxWjAXMRUwEwYD\n"
  "VQQDEwxvbnJlbmRlci5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASqhq19\n"
  "niZeSEXkw+fHQNdI+0Kb8+HTZ0cZ1v12VR4L4BsePBuFVv0Hrld7JwsONcS6LNpU\n"
  "Skt0ueRT4AC32Un3o4ICUzCCAk8wDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoG\n"
  "CCsGAQUFBwMBMAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYEFCCpL0lsHK+mqnL3OpMH\n"
  "JLh13h9TMB8GA1UdIwQYMBaAFJB3kjVnxP+ozKnme9mAeXvMk/k4MF4GCCsGAQUF\n"
  "BwEBBFIwUDAnBggrBgEFBQcwAYYbaHR0cDovL28ucGtpLmdvb2cvcy93ZTEvZkF3\n"
  "MCUGCCsGAQUFBzAChhlodHRwOi8vaS5wa2kuZ29vZy93ZTEuY3J0MCcGA1UdEQQg\n"
  "MB6CDG9ucmVuZGVyLmNvbYIOKi5vbnJlbmRlci5jb20wEwYDVR0gBAwwCjAIBgZn\n"
  "gQwBAgEwNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2MucGtpLmdvb2cvd2UxL3Qz\n"
  "TEpiWmlCdHNVLmNybDCCAQIGCisGAQQB1nkCBAIEgfMEgfAA7gB0AN3cyjSV1+EW\n"
  "BeeVMvrHn/g9HFDf2wA6FBJ2Ciysu8gqAAABl0afKbUAAAQDAEUwQwIfJXjIhpwc\n"
  "EAcu/D5sIgq6ZJytHJtfqyaXIfZeZZAiiQIgaaNOuYUNaHBoynZk+v1JLfjvcOa3\n"
  "Vlo1vtR3Ss3CGK4AdgAaBP9J0FQdQK/2oMO/8djEZy9O7O4jQGiYaxdALtyJfQAA\n"
  "AZdGnyrkAAAEAwBHMEUCIQCxnabwRsrPpfAuj9npD4qGTEwXA8wpkVKslfxp+mEj\n"
  "SAIgXeDsCV64uJo71hG0ueYnX9U22fybRJUfzmBDAnh9F0QwCgYIKoZIzj0EAwID\n"
  "SAAwRQIhAPdm2rxGNZ72Lz8vf0w/JgBno1yGlTX6fRsuvZIKMLMQAiBI+28KH8Rh\n"
  "4NVk/tjADsXiTmxKBWiFxgrAOKOmMUIOvQ==\n"
  "-----END CERTIFICATE-----\n";

  if (lastReceivedData == "[]") {
    Serial.println("No data to send.");
    return;
  }

  WiFiClientSecure client;
  client.setCACert(root_ca);

  HTTPClient https;

  Serial.println("Sending HTTP POST...");

  https.begin(apiEndpoint);
  https.addHeader("Content-Type", "application/json");

  Serial.printf("Response: %s\n", lastReceivedData.c_str());
  int httpResponseCode = https.POST(lastReceivedData);
  if (httpResponseCode > 0) {
    String response = https.getString();
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    Serial.printf("Response: %s\n", response.c_str());

    if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
      lastReceivedData = "[]";
      jsonDoc.clear();
    }
  } else {
    Serial.printf("Error on sending POST: %s\n", https.errorToString(httpResponseCode).c_str());
  }
  https.end();
}

void Base::processHttpQueue() {
  while (!messageQueue.empty()) {
    EspNowMessage msg = messageQueue.front();
    lastReceivedData = msg.json;
    sendHttp();
    messageQueue.pop();
  }
}

void Base::setupWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP-NOW-BASE", "12345678", 6);
  WiFi.begin(ssid, password);
  const int ledPin = 2;
  pinMode(ledPin, OUTPUT);
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    delay(500);
    Serial.print(".");
  }
  digitalWrite(ledPin, LOW);
  Serial.println("\nWiFi connected");
  String mac = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.println(mac);
  uint8_t primary;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("BASE Wi-Fi Channel: %d\n", primary);
}

void Base::sendEspNow() {
  int randomValue = random(0, 100);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char message[64];
  snprintf(message, sizeof(message), "{\"value\":%d,\"deviceId\":\"%s\"}", randomValue, macStr);

  // Use the current node in the cycle
  static size_t currentNodeIndex = 0;
  if (nodeMacs.empty()) return;

  addPeer(nodeMacs[currentNodeIndex].data());
  esp_err_t result = esp_now_send(nodeMacs[currentNodeIndex].data(), (uint8_t *)message, strlen(message));
  if (result == ESP_OK) {
    Serial.printf("ESP-NOW sent: %s\n", message);
  } else {
    Serial.println("ESP-NOW send failed");
  }

  // Move to next node
  currentNodeIndex = (currentNodeIndex + 1) % nodeMacs.size();
}

void Base::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void Base::onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len) {
  char buffer[256];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';

  Serial.printf("Received ESP-NOW: %s\n", buffer);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, buffer);
  if (!error) {
    int value = doc["value"];
    const char* deviceId = doc["deviceId"];

    //Serial.printf("Parsed JSON — Value: %d, Device ID: %s\n", value, deviceId);

    // Clear jsonDoc and create a new array
    jsonDoc.clear();
    JsonArray dataArray = jsonDoc.to<JsonArray>();
    JsonObject entry = dataArray.createNestedObject();
    entry["value"] = value;
    entry["deviceId"] = String(deviceId);

    // Serialize to lastReceivedData
    lastReceivedData = "";
    serializeJson(jsonDoc, lastReceivedData);

    // Add to message queue
    EspNowMessage msg;
    msg.json = lastReceivedData;
    msg.receivedAt = millis();
    messageQueue.push(msg);

    Serial.printf("Response: %s\n", lastReceivedData.c_str());
  } else {
    Serial.println("Failed to parse incoming JSON");
  }
}