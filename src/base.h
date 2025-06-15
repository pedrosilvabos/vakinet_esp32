#ifndef BASE_H
#define BASE_H

#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>
#include <array>
#include <queue>
#include <string>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define MAX_NODES 50
#define MAX_QUEUE_SIZE 100

class Base {
public:
  struct Message {
    char json[256]; // Fixed-size array to reduce fragmentation
    unsigned long receivedAt;
  };

  virtual void begin() = 0;   // Setup logic to implement in subclass
  virtual void update() = 0;  // Loop logic to implement in subclass
  static void setup();        // Initialize static members

  // Add a JSON message to the sending queue
  static void enqueueMessage(const char* json); // Static, uses const char*

protected:
  void setupWiFi();
  void processHttpQueue();
  void processLocalHttpQueue();
  void addNodeMac(const uint8_t* mac);
  void processMessageQueue();
  void checkHeap();

  static HTTPClient httpClient;
  static WiFiClientSecure secureClient;
  static WiFiClient wifiClient; // Reusable WiFiClient
  static std::vector<std::array<uint8_t, 6>> nodeMacs;
  static char lastReceivedData[1024];
  static StaticJsonDocument<1024> jsonDoc;
  static std::queue<Message> messageQueue;
};

#endif // BASE_H