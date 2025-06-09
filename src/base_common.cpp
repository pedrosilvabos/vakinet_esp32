#include "base.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>


// Define static members from Base class
std::vector<std::array<uint8_t, 6>> Base::nodeMacs;
String Base::lastReceivedData = "[]";
StaticJsonDocument<1024> Base::jsonDoc;
WiFiClientSecure Base::secureClient;
std::queue<Base::Message> Base::messageQueue;

void Base::setupWiFi() {
  WiFi.mode(WIFI_AP_STA);
  const char* ssid = "MEO-563920";
  const char* password = "346cbe99b8";

  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Synchronize time with NTP
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
      Serial.println("Time synchronized");
      Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("Failed to synchronize time");
    }
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void Base::processHttpQueue() {
  if (lastReceivedData == "[]") {
    return;
  }

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


  Base::secureClient.setCACert(GTS_ROOT_R4_CA);
  //client.setInsecure();

  HTTPClient https;

  Serial.println("Sending HTTP POST...");
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());  // Debug line added

  https.begin(secureClient, "https://vaquinet-api.onrender.com/esp/data");
  https.addHeader("Content-Type", "application/json");

  Serial.printf("Response: %s\n", lastReceivedData.c_str());
  int httpResponseCode = https.POST(lastReceivedData);
  if (httpResponseCode > 0) {
    String response = https.getString();
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    Serial.printf("Response: %s\n", response.c_str());

    if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
      lastReceivedData = "[]";
      jsonDoc.clear();
    }
  } else {
    Serial.printf("Error on sending POST: %s\n", https.errorToString(httpResponseCode).c_str());
  }
  https.end();
}