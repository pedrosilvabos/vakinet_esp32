//espNowRole.cpp
#include "EspNowRole.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#if defined(ROLE_BASE)
// We'll accumulate all received data as a JSON array string here
String EspNowRole::lastReceivedData = "[]";
StaticJsonDocument<1024> EspNowRole::jsonDoc;
std::queue<EspNowRole::EspNowMessage> EspNowRole::messageQueue;
#endif

void EspNowRole::setup() {
  setupWiFi();
  setupEspNow();

#if defined(ROLE_BASE)
  // Initialize JSON array for accumulating received data
  jsonDoc.clear();
   JsonArray dataArray = jsonDoc.to<JsonArray>();
#endif
}

const unsigned long sendInterval = 45000; // 45 seconds
unsigned long lastSendTime = 0;

void EspNowRole::loop() {
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();
#if defined(ROLE_NODE)
    sendEspNow();
#elif defined(ROLE_BASE)
    sendHttp();
#endif
  }
}

void EspNowRole::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void EspNowRole::onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len) {
  char buffer[256];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';

  Serial.printf("Received ESP-NOW: %s\n", buffer);

#if defined(ROLE_BASE)
  StaticJsonDocument<256> doc;
 DeserializationError error = deserializeJson(doc, buffer);
if (!error) {
  int value = doc["value"];
  const char* deviceId = doc["deviceId"];  // Temporary pointer — do not store directly

  Serial.printf("Parsed JSON — Value: %d, Device ID: %s\n", value, deviceId);

  if (jsonDoc.isNull()) {
    jsonDoc.to<JsonArray>();
  }

  JsonArray dataArray = jsonDoc.as<JsonArray>();
  JsonObject entry = dataArray.createNestedObject();
  entry["value"] = value;
  entry["deviceId"] = String(deviceId); // 🔧 This is key — copies the string safely

  lastReceivedData = "";
  serializeJson(jsonDoc, lastReceivedData);

  Serial.printf("Response22: %s\n", lastReceivedData.c_str());
}
 else {
    Serial.println("Failed to parse incoming JSON");
  }
#endif
}


void EspNowRole::setupWiFi() {
#if defined(ROLE_BASE)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP-NOW-BASE", "12345678", 6);
  WiFi.begin(ssid, password);
  const int ledPin = 2;
  pinMode(ledPin, OUTPUT);
  esp_now_register_send_cb(EspNowRole::onDataSent);
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    delay(500);
    Serial.print(".");
  }
  digitalWrite(ledPin, LOW);
  Serial.println("\nWiFi connected");

  uint8_t primary;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("BASE Wi-Fi Channel: %d\n", primary);

#elif defined(ROLE_NODE)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  String baseMacStr = "AA:BB:CC:DD:EE:FF"; // Replace with BASE MAC
  int n = WiFi.scanNetworks(false, true);
  bool found = false;
  for (int i = 0; i < n; ++i) {
    if (WiFi.BSSIDstr(i).equalsIgnoreCase(baseMacStr)) {
      int chan = WiFi.channel(i);
      esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);
      Serial.printf("Found BASE at channel %d and synced\n", chan);
      found = true;
      break;
    }
  }
  if (!found) {
    Serial.println("BASE not found. Defaulting to channel 6");
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
  }

  uint8_t chan;
  wifi_second_chan_t _;
  esp_wifi_get_channel(&chan, &_);
  Serial.printf("NODE Wi-Fi Channel: %d\n", chan);
#endif
}

void EspNowRole::setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(EspNowRole::onDataSent);
  esp_now_register_recv_cb(EspNowRole::onReceiveEspNow);

#if defined(ROLE_NODE)
  esp_now_peer_info_t peerInfo = {};
  uint8_t baseMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Replace with actual BASE MAC
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
#endif
}

void EspNowRole::sendEspNow() {
  int randomValue = random(0, 100);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char message[64];
  snprintf(message, sizeof(message), "{\"value\":%d,\"deviceId\":\"%s\"}", randomValue, macStr);

  uint8_t baseMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  esp_err_t result = esp_now_send(baseMac, (uint8_t *)message, strlen(message));
  if (result == ESP_OK) {
    Serial.printf("ESP-NOW sent: %s\n", message);
  } else {
    Serial.println("ESP-NOW send failed");
  }
}

void EspNowRole::sendHttp() {
#if defined(ROLE_BASE)


const char root_ca[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDpTCCA0ugAwIBAgIQfAwp5EhN0tkNy410DD/sjzAKBggqhkjOPQQDAjA7MQsw\n"
"CQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMQwwCgYD\n"
"VQQDEwNXRTEwHhcNMjUwNjA2MTgwMjA0WhcNMjUwOTA0MTkwMjAxWjAXMRUwEwYD\n"
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

   https.begin("https://vaquinet-api.onrender.com/esp/data");
    https.addHeader("Content-Type", "application/json");

    Serial.printf("Response:", lastReceivedData);
    int httpResponseCode = https.POST(lastReceivedData);
    if (httpResponseCode > 0) {
      String response = https.getString();
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
      Serial.printf("Response: %s\n", response.c_str());

      if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
        // Clear the accumulated data on successful send
        lastReceivedData = "[]";
        jsonDoc.clear();
      }
    } else {
      Serial.printf("Error on sending POST: %s\n", https.errorToString(httpResponseCode).c_str());
    }
    https.end();
  
#endif
}

