#include <Arduino.h>
#include "laststate/latch.hpp"

void setup() {
    Serial.begin(115200);
    Serial.printf("Latch %s: %s\n", LS_VERSION_STRING,
                  ls_build_id_validate(ls_build_id()) ? "linked" : "invalid build ID");
}

void loop() {
    delay(1000);
}
