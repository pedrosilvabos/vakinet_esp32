#ifndef BASE_ESPNOW_H
#define BASE_ESPNOW_H

#include "base.h"
#include <esp_now.h>
#include <WiFi.h>

class EspNowBase : public Base {
public:
  void begin() override;
  void update() override;

private:
  void setupEspNow();
  void addPeer(const uint8_t* mac_addr);
  void removePeer(const uint8_t* mac_addr);
  void sendTriggerToNode(const uint8_t* nodeId);
  static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
  static void onReceiveEspNow(const uint8_t* mac, const uint8_t* incomingData, int len);
};

#endif