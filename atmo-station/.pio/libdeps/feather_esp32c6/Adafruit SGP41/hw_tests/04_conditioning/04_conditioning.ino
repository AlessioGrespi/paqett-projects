/**
 * @file 04_conditioning.ino
 *
 * Arduino hardware test for the Sensirion SGP41 conditioning command.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;

static bool srawValid(uint16_t value) {
  return (value != 0x0000) && (value != 0xFFFF);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 04: conditioning");

  bool begin_ok = sgp41.begin();
  if (!begin_ok) {
    Serial.println("FAIL: begin");
    Serial.println("TEST 04: FAIL");
    while (1) {
      delay(1000);
    }
  }

  bool test1_ok = false;
  bool test2_ok = false;

  uint16_t sraw_voc = 0;
  if (sgp41.executeConditioning(&sraw_voc)) {
    Serial.print("sraw_voc default: 0x");
    Serial.println(sraw_voc, HEX);
    test1_ok = srawValid(sraw_voc);
  } else {
    Serial.println("FAIL: executeConditioning default");
  }
  Serial.print("Test 1 (default): ");
  Serial.println(test1_ok ? "PASS" : "FAIL");

  uint16_t sraw_voc2 = 0;
  if (sgp41.executeConditioning(&sraw_voc2, 70.0, 30.0)) {
    Serial.print("sraw_voc 70%/30C: 0x");
    Serial.println(sraw_voc2, HEX);
    test2_ok = srawValid(sraw_voc2);
  } else {
    Serial.println("FAIL: executeConditioning explicit");
  }
  Serial.print("Test 2 (70%/30C): ");
  Serial.println(test2_ok ? "PASS" : "FAIL");

  if (test1_ok && test2_ok) {
    Serial.println("TEST 04: PASS");
  } else {
    Serial.println("TEST 04: FAIL");
  }
}

void loop() { delay(1000); }
