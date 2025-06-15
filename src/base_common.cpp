#include "base.h"
#include <time.h>

// Static members definition
HTTPClient Base::httpClient;
WiFiClientSecure Base::secureClient;
WiFiClient Base::wifiClient; // Reusable WiFiClient
std::vector<std::array<uint8_t, 6>> Base::nodeMacs;
char Base::lastReceivedData[1024] = "[]";
StaticJsonDocument<1024> Base::jsonDoc;
std::queue<Base::Message> Base::messageQueue;

void Base::setup() {
    // Pre-allocate vector capacity to avoid fragmentation
    nodeMacs.reserve(MAX_NODES);
}

void Base::setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
    const char* ssid = "MEO-563920";
    const char* password = "346cbe99b8";
    int attempts = 0;
    const int maxAttempts = 5;

    while (attempts < maxAttempts) {
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
            return;
        }
        Serial.println("\nWiFi connection failed, retrying in 10 seconds");
        WiFi.disconnect();
        delay(10000);
        attempts++;
    }
    Serial.println("WiFi connection failed after max attempts");
}

void Base::processLocalHttpQueue() {
    static bool wasEmpty = false;

    if (lastReceivedData == nullptr || strlen(lastReceivedData) < 5 || 
        strcmp(lastReceivedData, "[]") == 0 || strstr(lastReceivedData, "[null]")) {
        if (!wasEmpty) {
            Serial.println("[HTTP] Skipping empty or invalid data.");
            wasEmpty = true;
        }
        return;
    }

    wasEmpty = false;

    Serial.println("======== DEBUG START ========");
    Serial.print("[HTTP] ESP32 local IP: ");
    Serial.println(WiFi.localIP());

    const char* host = "192.168.1.87";
    const int port = 10001;
    const char* endpoint = "/esp/data";

    Serial.print("[HTTP] Connecting to ");
    Serial.print("http://");
    Serial.print(host);
    Serial.print(":");
    Serial.print(port);
    Serial.println(endpoint);

    // Test raw TCP socket
    if (!wifiClient.connect(host, port)) {
        Serial.println("✖️ [TCP] Raw socket connect() failed");
    } else {
        Serial.println("✔️ [TCP] Raw socket connect() OK");
        wifiClient.stop(); // Close so HTTPClient can use it
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", host, port, endpoint);

    Serial.println("[HTTP] Starting HTTPClient...");
    httpClient.setTimeout(5000); // 5-second timeout
    bool began = httpClient.begin(wifiClient, url);
    if (!began) {
        Serial.println("✖️ [HTTP] httpClient.begin() failed");
        httpClient.end();
        wifiClient.stop();
        return;
    }

    Serial.println("✔️ [HTTP] httpClient.begin() succeeded");
    httpClient.addHeader("Content-Type", "application/json");

    Serial.print("[HTTP] POSTing payload: ");
    Serial.println(lastReceivedData);

    int httpResponseCode = httpClient.POST(lastReceivedData);

    if (httpResponseCode > 0) {
        Serial.printf("✔️ [HTTP] POST success! Code: %d\n", httpResponseCode);

        char buffer[512];
        WiFiClient* stream = httpClient.getStreamPtr();
        if (stream && stream->available()) {
            unsigned long start = millis();
            int readLen = 0;
            while (stream->available() && millis() - start < 5000) {
                readLen = stream->readBytes(buffer, sizeof(buffer) - 1);
                buffer[readLen] = '\0';
            }
            Serial.print("[HTTP] Response body: ");
            Serial.println(buffer);
        }

        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
            jsonDoc.clear();
            strcpy(lastReceivedData, "[]");
            Serial.println("[HTTP] Payload acknowledged, cleared lastReceivedData.");
        }
    } else {
        Serial.printf("✖️ [HTTP] POST failed! Code: %d, Error: %s\n",
                      httpResponseCode,
                      httpClient.errorToString(httpResponseCode).c_str());
        jsonDoc.clear();
    }

    httpClient.end();
    wifiClient.stop();

    Serial.printf("[HTTP] Free heap after request: %u\n", ESP.getFreeHeap());
    Serial.println("======== DEBUG END ========");
}

void Base::processHttpQueue() {
    static bool wasEmpty = false;

    if (lastReceivedData == nullptr || strlen(lastReceivedData) < 5 ||
        strcmp(lastReceivedData, "[]") == 0 || strstr(lastReceivedData, "[null]")) {
        if (!wasEmpty) {
            Serial.println("[HTTPS] Skipping empty or invalid data.");
            wasEmpty = true;
        }
        return;
    }

    wasEmpty = false;

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

    Serial.println("======== HTTPS DEBUG START ========");
    Serial.print("[HTTPS] ESP32 local IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("[HTTPS] Starting HTTPClient...");

    httpClient.setTimeout(5000);
    bool began = httpClient.begin(secureClient, "https://vaquinet-api.onrender.com/esp/data");
    if (!began) {
        Serial.println("✖️ [HTTPS] httpClient.begin() failed");
        httpClient.end();
        secureClient.stop();
        return;
    }

    Serial.println("✔️ [HTTPS] httpClient.begin() succeeded");
    httpClient.addHeader("Content-Type", "application/json");

    Serial.print("[HTTPS] POSTing payload: ");
    Serial.println(lastReceivedData);

    int httpResponseCode = httpClient.POST(lastReceivedData);

    if (httpResponseCode > 0) {
        Serial.printf("✔️ [HTTPS] POST success! Code: %d\n", httpResponseCode);

        char buffer[512];
        WiFiClient* stream = httpClient.getStreamPtr();
        if (stream && stream->available()) {
            unsigned long start = millis();
            int readLen = stream->readBytes(buffer, sizeof(buffer) - 1);
            buffer[readLen] = '\0';
            Serial.print("[HTTPS] Response body: ");
            Serial.println(buffer);
        }

        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
            jsonDoc.clear();
            strcpy(lastReceivedData, "[]");
            Serial.println("[HTTPS] Payload acknowledged, cleared lastReceivedData.");
        }
    } else {
        Serial.printf("✖️ [HTTPS] POST failed! Code: %d, Error: %s\n",
                      httpResponseCode,
                      httpClient.errorToString(httpResponseCode).c_str());
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

        Serial.print("[Queue] Processing msg: ");
        Serial.println(msg.json);

        StaticJsonDocument<512> tempDoc;
        DeserializationError error = deserializeJson(tempDoc, msg.json);
        if (error) {
            Serial.printf("❌ Failed to parse JSON: %s\n", error.c_str());
            continue;
        }

        JsonObject obj = tempDoc.as<JsonObject>();
        jsonArray.add(obj);
        Serial.println("✅ Added to JSON array.");
    }

    if (jsonArray.size() == 0) {
        Serial.println("[Queue] All messages were invalid, skipping serialization.");
        strcpy(lastReceivedData, "[]");
        return;
    }

    if (measureJson(jsonDoc) >= sizeof(lastReceivedData)) {
        Serial.println("❌ JSON too large for lastReceivedData");
        strcpy(lastReceivedData, "[]");
        return;
    }

    if (serializeJson(jsonDoc, lastReceivedData, sizeof(lastReceivedData)) == 0) {
        Serial.println("❌ Failed to serialize to lastReceivedData");
        strcpy(lastReceivedData, "[]");
    } else {
        Serial.print("✅ Final serialized JSON: ");
        Serial.println(lastReceivedData);
    }

    // Optional cleanup if queue still bloated
    if (messageQueue.size() > MAX_QUEUE_SIZE) {
        Serial.println("⚠️ Queue overflow, trimming...");
        while (messageQueue.size() > MAX_QUEUE_SIZE / 2) {
            messageQueue.pop();
        }
    }
}

void Base::checkHeap() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 60000) {
        Serial.printf("Free heap: %u, Max alloc heap: %u\n", 
                      ESP.getFreeHeap(), ESP.getMaxAllocHeap());
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