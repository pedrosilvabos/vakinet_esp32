#include <Arduino.h>

#ifdef ROLE_BASE
#include "base.h"
Base base;
#elif defined(ROLE_NODE)
#include "node.h"
Node node;
#else
#error "No ROLE defined! Use -DROLE_BASE or -DROLE_NODE"
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
