/**
 * @file 12_repeated_conditioning.ino
 *
 * Hardware test for repeated SGP41 conditioning calls.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 12: repeated_conditioning");

  bool begin_ok = sgp.begin();
  Serial.print("begin(): ");
  Serial.println(begin_ok ? "PASS" : "FAIL");

  bool conditioning_ok[5] = {false, false, false, false, false};
  uint16_t sraw_voc[5] = {0, 0, 0, 0, 0};

  for (uint8_t i = 0; i < 5; i++) {
    delay(1000);
    conditioning_ok[i] = sgp.executeConditioning(&sraw_voc[i]);
    Serial.print("conditioning ");
    Serial.print(i + 1);
    Serial.print(" sraw_voc: ");
    Serial.println(sraw_voc[i]);
  }

  bool all_conditioning_ok = true;
  bool all_values_ok = true;
  for (uint8_t i = 0; i < 5; i++) {
    if (!conditioning_ok[i]) {
      all_conditioning_ok = false;
    }
    if (sraw_voc[i] == 0 || sraw_voc[i] == 0xFFFF) {
      all_values_ok = false;
    }
  }

  Serial.print("conditioning calls: ");
  Serial.println(all_conditioning_ok ? "PASS" : "FAIL");
  Serial.print("conditioning values: ");
  Serial.println(all_values_ok ? "PASS" : "FAIL");

  uint16_t sraw_voc_after = 0;
  uint16_t sraw_nox_after = 0;
  bool measure_ok = sgp.measureRawSignals(&sraw_voc_after, &sraw_nox_after);
  Serial.print("measureRawSignals(): ");
  Serial.println(measure_ok ? "PASS" : "FAIL");

  bool overall = begin_ok && all_conditioning_ok && all_values_ok && measure_ok;
  if (overall) {
    Serial.println("TEST 12: PASS");
  } else {
    Serial.println("TEST 12: FAIL");
  }
}

void loop() {
  delay(1000);
}
