#ifndef NODE_ESPNOW_H
#define NODE_ESPNOW_H

#include "node.h"
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

class EspNowNode : public Node {
public:
  void begin() override;
  void update() override;

private:
  void setupEspNow();
  void sendEspNow();
  static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
  static void onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len);
  void setupWiFi();
};

#endif