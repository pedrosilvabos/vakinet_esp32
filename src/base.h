#ifndef BASE_H
#define BASE_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <queue>
#include <ArduinoJson.h>  // Make sure to include ArduinoJson header

// Define TriggerMessage struct globally
struct TriggerMessage {
  uint8_t command;  // e.g., 1 for triggerSend
};

class Base {
public:
  void begin();
  void update();

  static void onReceiveEspNow(const uint8_t *mac, const uint8_t *incomingData, int len);
  static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

  static StaticJsonDocument<1024> jsonDoc;  // Keep static JsonDocument only

  static String lastReceivedData;
  struct EspNowMessage {
    String json;
    unsigned long receivedAt;
  };
  static std::queue<EspNowMessage> messageQueue;

  const char* ssid = "MEO-563920";
  const char* password = "346cbe99b8";
  const char* apiEndpoint = "https://vaquinet-api.onrender.com/esp/data";

private:
  void setupWiFi();
  void setupEspNow();
  void sendEspNow();
  void processHttpQueue();
  void sendHttp();
  static void sendTriggerToNode(const uint8_t* mac_addr);
  static void addPeer(const uint8_t* mac_addr);
  static void removePeer(const uint8_t* mac_addr);

  static std::vector<std::array<uint8_t, 6>> nodeMacs;
};

#endif