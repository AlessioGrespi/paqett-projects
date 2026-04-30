// SPDX-FileCopyrightText: 2026 Limor Fried for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*
 * Arduino library for the Sensirion SGP41 multi-pixel gas sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;
Adafruit_SGP41 sgp41_bad;

static void printResult(const char *label, bool pass) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(pass ? "PASS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 01: begin");

  bool pass = true;

  bool ok_begin = sgp41.begin();
  printResult("begin()", ok_begin);
  pass &= ok_begin;

  bool ok_reinit = sgp41.begin();
  printResult("begin() reinit", ok_reinit);
  pass &= ok_reinit;

  bool ok_bad = sgp41_bad.begin(0x10);
  printResult("begin(0x10)", !ok_bad);
  pass &= !ok_bad;

  if (pass) {
    Serial.println("TEST 01: PASS");
  } else {
    Serial.println("TEST 01: FAIL");
  }

  while (1) {
    delay(1000);
  }
}

void loop() {}
