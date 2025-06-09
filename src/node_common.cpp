#include "node.h"
#include <arduino.h>

// Define static members from Node class
bool Node::blinking = false;
unsigned long Node::blinkStartTime = 0;

// Manages LED blinking for node roles (ESP-NOW and LoRa).
// Turns off the LED after the blink duration expires.
void Node::updateBlink() {
  if (blinking) {
    if (millis() - blinkStartTime >= blinkDuration) {
      digitalWrite(LED_PIN, LOW);
      blinking = false;
      Serial.println("LED turned off after blink");
    }
  }
}