/**
 * @file 03_self_test.ino
 *
 * Arduino hardware test for the Sensirion SGP41 self-test.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp;

static void printPassFail(const __FlashStringHelper *label, bool pass) {
  Serial.print(label);
  Serial.print(F(": "));
  Serial.println(pass ? F("PASS") : F("FAIL"));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("TEST 03: self_test"));

  bool begin_ok = sgp.begin();
  printPassFail(F("begin"), begin_ok);
  if (!begin_ok) {
    Serial.println(F("TEST 03: FAIL"));
    return;
  }

  uint16_t result1 = sgp.executeSelfTest();
  Serial.print(F("self-test result: 0x"));
  if (result1 < 0x1000) {
    Serial.print('0');
  }
  Serial.println(result1, HEX);

  bool result_ok = (result1 == SGP41_SELF_TEST_OK);
  bool voc_ok = ((result1 & SGP41_SELF_TEST_VOC_FAIL_MASK) == 0);
  bool nox_ok = ((result1 & SGP41_SELF_TEST_NOX_FAIL_MASK) == 0);

  printPassFail(F("self-test value"), result_ok);
  printPassFail(F("VOC pixel"), voc_ok);
  printPassFail(F("NOx pixel"), nox_ok);

  uint16_t result2 = sgp.executeSelfTest();
  bool consistent = (result2 == result1);
  Serial.print(F("self-test result 2: 0x"));
  if (result2 < 0x1000) {
    Serial.print('0');
  }
  Serial.println(result2, HEX);
  printPassFail(F("consistency"), consistent);

  bool overall = begin_ok && result_ok && voc_ok && nox_ok && consistent;
  Serial.println(overall ? F("TEST 03: PASS") : F("TEST 03: FAIL"));
}

void loop() {
  delay(1000);
}
