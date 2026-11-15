#include <AUnit.h>

void setup() {
    Serial.begin(115200);
}

void loop() {
    aunit::TestRunner::run();
}
