#ifndef NODE_H
#define NODE_H

#define LED_PIN 2
#define BLINK_DURATION 1000

class Node {
public:
  virtual void begin() = 0;
  virtual void update() = 0;

protected:
  void updateBlink();

  static bool blinking;
  static unsigned long blinkStartTime;
  static const int blinkDuration = BLINK_DURATION;
  static const int ledPin = LED_PIN;
};

#endif