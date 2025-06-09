//espNowRole.h
#ifndef ESP_NOW_ROLE_H
#define ESP_NOW_ROLE_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <queue>
#include <ArduinoJson.h>  // Make sure to include ArduinoJson header

class EspNowRole {
public:
    void setup();
    void loop();

    static void onReceiveEspNow(const uint8_t *mac, const uint8_t *incomingData, int len);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

    static StaticJsonDocument<1024> jsonDoc;  // Keep static JsonDocument only

#if defined(ROLE_BASE)
    static String lastReceivedData;
    struct EspNowMessage {
      String json;
      unsigned long receivedAt;
    };
    static std::queue<EspNowMessage> messageQueue;

    const char* ssid = "MEO-563920";
    const char* password = "346cbe99b8";
    const char* apiEndpoint = "https://vaquinet-api.onrender.com/esp/data";

    
#endif

private:
    void setupWiFi();
    void setupEspNow();
    void sendEspNow();
    void processHttpQueue();
    void sendHttp();

    unsigned long lastSendTime = 0;
    const unsigned long sendInterval = 10000; // 10 seconds
};

#endif // ESP_NOW_ROLE_H
