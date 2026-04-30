/**************************************************************************/
/*!
 * @file 13_measure_rate.ino
 *
 * Arduino hardware test for the Sensirion SGP41 gas sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */
/**************************************************************************/

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;

static bool valid_raw(uint16_t value) {
  return (value != 0) && (value != 0xFFFF);
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 13: measure_rate");

  bool pass = true;

  if (!sgp41.begin()) {
    Serial.println("FAIL: begin()");
    Serial.println("TEST 13: FAIL");
    return;
  }

  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (!sgp41.executeConditioning(&sraw_voc)) {
    Serial.println("FAIL: executeConditioning()");
    Serial.println("TEST 13: FAIL");
    return;
  }

  const uint8_t kSamples = 10;
  uint32_t durations[kSamples] = {0};
  bool sample_ok[kSamples] = {false};
  uint32_t total_us = 0;
  bool all_valid = true;
  bool all_under_200ms = true;

  for (uint8_t i = 0; i < kSamples; i++) {
    uint32_t start_us = micros();
    bool ok = sgp41.measureRawSignals(&sraw_voc, &sraw_nox);
    uint32_t elapsed_us = micros() - start_us;

    durations[i] = elapsed_us;
    total_us += elapsed_us;

    bool valid = ok && valid_raw(sraw_voc) && valid_raw(sraw_nox);
    sample_ok[i] = valid;
    if (!valid) {
      all_valid = false;
    }

    if (elapsed_us >= 200000UL) {
      all_under_200ms = false;
    }

    Serial.print("measurement #");
    Serial.print(i + 1);
    Serial.print(" duration (us): ");
    Serial.println(elapsed_us);
  }

  uint32_t average_us = total_us / kSamples;

  Serial.print("average duration (us): ");
  Serial.println(average_us);

  if (all_under_200ms) {
    Serial.println("PASS: each measurement < 200ms");
  } else {
    Serial.println("FAIL: each measurement < 200ms");
    pass = false;
  }

  if (average_us < 100000UL) {
    Serial.println("PASS: average < 100ms");
  } else {
    Serial.println("FAIL: average < 100ms");
    pass = false;
  }

  if (all_valid) {
    Serial.println("PASS: all measurements valid");
  } else {
    Serial.println("FAIL: all measurements valid");
    pass = false;
  }

  for (uint8_t i = 0; i < kSamples; i++) {
    Serial.print("sample #");
    Serial.print(i + 1);
    Serial.print(" valid: ");
    Serial.println(sample_ok[i] ? "PASS" : "FAIL");
  }

  if (pass) {
    Serial.println("TEST 13: PASS");
  } else {
    Serial.println("TEST 13: FAIL");
  }
}

void loop(void) {
  delay(1000);
}
