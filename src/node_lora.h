#ifndef NODE_LORA_H
#define NODE_LORA_H

#include "node.h"
#include <LoRa.h>
#include <ArduinoJson.h>

class LoRaNode : public Node {
public:
  void begin() override;
  void update() override;

private:
  void setupLoRa();
  void sendEspNow();
  static void onReceiveLoRa(int packetSize);
  void setupWiFi();
};

#endif