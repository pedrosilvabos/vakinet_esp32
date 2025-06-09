#ifndef NODE_H
#define NODE_H
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>

class Node {
public:
    static void begin();
    static void update();
    static void triggerSend();
    static void onReceiveEspNow(const uint8_t *mac, const uint8_t *incomingData, int len);
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

private:
    static const uint8_t LED_PIN = 2;
    static bool blinking;
    static unsigned long blinkStartTime;
    static const unsigned long blinkDuration = 100; // milliseconds
    static const unsigned long sendInterval = 10000; // 10 seconds

    static void setupWiFi();
    static void setupEspNow();
    static void sendEspNow();
    static void updateBlink();
};

#endif