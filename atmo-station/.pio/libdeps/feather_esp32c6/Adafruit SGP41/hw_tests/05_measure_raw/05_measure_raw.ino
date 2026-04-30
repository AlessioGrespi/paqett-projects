/**************************************************************************/
/*!
 * @file 05_measure_raw.ino
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

  Serial.println("TEST 05: measure_raw");

  bool pass = true;

  if (!sgp41.begin()) {
    Serial.println("FAIL: begin()");
    Serial.println("TEST 05: FAIL");
    return;
  }

  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (!sgp41.executeConditioning(&sraw_voc)) {
    Serial.println("FAIL: executeConditioning()");
    Serial.println("TEST 05: FAIL");
    return;
  }

  bool ok = sgp41.measureRawSignals(&sraw_voc, &sraw_nox);
  if (!ok) {
    Serial.println("FAIL: measureRawSignals() #1");
    pass = false;
  }

  Serial.print("sraw_voc #1: ");
  Serial.print(sraw_voc);
  Serial.print(" (0x");
  Serial.print(sraw_voc, HEX);
  Serial.println(")");

  Serial.print("sraw_nox #1: ");
  Serial.print(sraw_nox);
  Serial.print(" (0x");
  Serial.print(sraw_nox, HEX);
  Serial.println(")");

  if (!valid_raw(sraw_voc)) {
    Serial.println("FAIL: sraw_voc invalid #1");
    pass = false;
  } else {
    Serial.println("PASS: sraw_voc valid #1");
  }

  if (!valid_raw(sraw_nox)) {
    Serial.println("FAIL: sraw_nox invalid #1");
    pass = false;
  } else {
    Serial.println("PASS: sraw_nox valid #1");
  }

  ok = sgp41.measureRawSignals(&sraw_voc, &sraw_nox);
  if (!ok) {
    Serial.println("FAIL: measureRawSignals() #2");
    pass = false;
  }

  Serial.print("sraw_voc #2: ");
  Serial.print(sraw_voc);
  Serial.print(" (0x");
  Serial.print(sraw_voc, HEX);
  Serial.println(")");

  Serial.print("sraw_nox #2: ");
  Serial.print(sraw_nox);
  Serial.print(" (0x");
  Serial.print(sraw_nox, HEX);
  Serial.println(")");

  if (!valid_raw(sraw_voc)) {
    Serial.println("FAIL: sraw_voc invalid #2");
    pass = false;
  } else {
    Serial.println("PASS: sraw_voc valid #2");
  }

  if (!valid_raw(sraw_nox)) {
    Serial.println("FAIL: sraw_nox invalid #2");
    pass = false;
  } else {
    Serial.println("PASS: sraw_nox valid #2");
  }

  if (pass) {
    Serial.println("TEST 05: PASS");
  } else {
    Serial.println("TEST 05: FAIL");
  }
}

void loop(void) {
  delay(1000);
}
