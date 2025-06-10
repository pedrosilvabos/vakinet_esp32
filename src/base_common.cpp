#include "base.h"
#include <time.h>

// Static members definition
HTTPClient Base::httpClient;
WiFiClientSecure Base::secureClient;
std::vector<std::array<uint8_t, 6>> Base::nodeMacs;
char Base::lastReceivedData[1024] = "[]";
StaticJsonDocument<1024> Base::jsonDoc;
std::queue<Base::Message> Base::messageQueue;

void Base::setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
    const char* ssid = "MEO-563920";
    const char* password = "346cbe99b8";

    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(50);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        configTime(0, 0, "pool.ntp.org");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000)) {
            Serial.println("Time synchronized");
        } else {
            Serial.println("Failed to synchronize time");
        }
    } else {
        Serial.println("\nWiFi connection failed, retrying in 10 seconds");
        WiFi.disconnect();
        delay(10000);
        setupWiFi();
    }
}

void Base::processHttpQueue() {
    if (!strcmp(lastReceivedData, "[]")) {
        return;
    }

    static bool certSet = false;
    if (!certSet) {
        const char* GTS_ROOT_R4_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
        )EOF";
        secureClient.setCACert(GTS_ROOT_R4_CA);
        certSet = true;
    }

    Serial.println("Sending HTTP POST...");
    char buffer[512];
    memset(buffer, 0, sizeof(buffer));

    bool began = httpClient.begin(secureClient, "https://vaquinet-api.onrender.com/esp/data");
    if (!began) {
        Serial.println("HTTPS begin failed");
        httpClient.end();
        secureClient.stop();
        return;
    }

    httpClient.addHeader("Content-Type", "application/json");

    int httpResponseCode = httpClient.POST(lastReceivedData);

    if (httpResponseCode > 0) {
        WiFiClient* stream = httpClient.getStreamPtr();
        if (stream) {
            while (stream->available()) {
                int readLen = stream->readBytes(buffer, sizeof(buffer) - 1);
                buffer[readLen] = '\0';
            }
        }
        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
            jsonDoc.clear();
            strcpy(lastReceivedData, "[]");
        }
    } else {
        Serial.printf("POST failed: %s\n", httpClient.errorToString(httpResponseCode).c_str());
        jsonDoc.clear();
    }

    httpClient.end();
    secureClient.stop();

    Serial.printf("Free heap after request: %u\n", ESP.getFreeHeap());
}

void Base::enqueueMessage(const char* json) {
    if (messageQueue.size() >= MAX_QUEUE_SIZE) {
        Serial.println("Queue full, dropping oldest message");
        messageQueue.pop();
    }
    Message msg;
    strncpy(msg.json, json, sizeof(msg.json) - 1);
    msg.json[sizeof(msg.json) - 1] = '\0';
    msg.receivedAt = millis();
    messageQueue.push(msg);
}

void Base::addNodeMac(const uint8_t* mac) {
    std::array<uint8_t, 6> macArray;
    memcpy(macArray.data(), mac, 6);
    
    if (std::find(nodeMacs.begin(), nodeMacs.end(), macArray) == nodeMacs.end()) {
        if (nodeMacs.size() >= MAX_NODES) {
            nodeMacs.erase(nodeMacs.begin());
        }
        nodeMacs.push_back(macArray);
    }
}

void Base::processMessageQueue() {
    if (messageQueue.empty()) {
        return;
    }

    jsonDoc.clear();
    JsonArray jsonArray = jsonDoc.to<JsonArray>();

    while (!messageQueue.empty() && messageQueue.size() <= MAX_QUEUE_SIZE) {
        Message msg = messageQueue.front();
        messageQueue.pop();

        StaticJsonDocument<512> tempDoc;
        DeserializationError error = deserializeJson(tempDoc, msg.json);
        if (!error) {
            jsonArray.add(tempDoc.as<JsonObject>());
        } else {
            Serial.printf("Failed to parse message JSON: %s\n", error.c_str());
        }
    }

    if (serializeJson(jsonDoc, lastReceivedData, sizeof(lastReceivedData)) == 0) {
        Serial.println("Failed to serialize JSON to lastReceivedData");
        strcpy(lastReceivedData, "[]");
    }

    if (messageQueue.size() > MAX_QUEUE_SIZE) {
        Serial.println("Queue overflow, clearing oldest messages");
        while (messageQueue.size() > MAX_QUEUE_SIZE / 2) {
            messageQueue.pop();
        }
    }
}

void Base::checkHeap() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 60000) {
        Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
        if (ESP.getFreeHeap() < 10000) {
            Serial.println("Warning: Low heap memory");
            nodeMacs.clear();
            while (!messageQueue.empty()) messageQueue.pop();
            jsonDoc.clear();
            strcpy(lastReceivedData, "[]");
        }
        lastCheck = millis();
    }
}