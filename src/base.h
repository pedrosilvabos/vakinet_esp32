#ifndef BASE_H
#define BASE_H

#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>
#include <array>
#include <queue>
#include <string>

class Base {
public:
  struct Message {
    String json;
    unsigned long receivedAt;
  };

  virtual void begin() = 0;
  virtual void update() = 0;

protected:
  void setupWiFi();
  void processHttpQueue();

  static std::vector<std::array<uint8_t, 6>> nodeMacs;
  static String lastReceivedData;
  static StaticJsonDocument<1024> jsonDoc;
  static std::queue<Message> messageQueue;
};

#endif