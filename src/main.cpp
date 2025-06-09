#include <Arduino.h>
#ifdef ROLE_BASE
  #ifdef PROTOCOL_ESPNOW
    #include "base_espnow.h"
    EspNowBase base;
  #elif defined(PROTOCOL_LORA)
    #include "base_lora.h"
    LoRaBase base;
  #endif
#elif defined(ROLE_NODE)
  #ifdef PROTOCOL_ESPNOW
    #include "node_espnow.h"
    EspNowNode node;
  #elif defined(PROTOCOL_LORA)
    #include "node_lora.h"
    LoRaNode node;
  #endif
#endif

void setup() {
  Serial.begin(115200);
  #ifdef ROLE_BASE
    base.begin();
  #elif defined(ROLE_NODE)
    node.begin();
  #endif
}

void loop() {
  #ifdef ROLE_BASE
    base.update();
  #elif defined(ROLE_NODE)
    node.update();
  #endif
}