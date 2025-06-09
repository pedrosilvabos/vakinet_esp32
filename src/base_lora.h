#ifndef BASE_LORA_H
#define BASE_LORA_H

#include "base.h"
#include <LoRa.h>

class LoRaBase : public Base {
public:
  void begin() override;
  void update() override;

private:
  void setupLoRa();
  void sendTriggerToNode(const uint8_t* nodeId);
  static void onReceiveLoRa(int packetSize);
};

#endif