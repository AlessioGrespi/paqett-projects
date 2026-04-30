// SPDX-FileCopyrightText: 2026 Limor Fried for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp;

static void printSerial(const uint16_t serial[3]) {
  Serial.print("Serial: 0x");
  Serial.print(serial[0], HEX);
  Serial.print(" 0x");
  Serial.print(serial[1], HEX);
  Serial.print(" 0x");
  Serial.println(serial[2], HEX);
}

static bool serialMatches(const uint16_t a[3], const uint16_t b[3]) {
  return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 11: soft_reset");

  bool overall_pass = true;

  bool ok_begin = sgp.begin();
  Serial.print("begin(): ");
  Serial.println(ok_begin ? "PASS" : "FAIL");
  if (!ok_begin) {
    overall_pass = false;
  }

  uint16_t serial_before[3] = {0, 0, 0};
  bool ok_serial_before = sgp.getSerialNumber(serial_before);
  Serial.print("getSerialNumber (before): ");
  Serial.println(ok_serial_before ? "PASS" : "FAIL");
  if (ok_serial_before) {
    printSerial(serial_before);
  } else {
    overall_pass = false;
  }

  bool ok_reset = sgp.softReset();
  Serial.print("softReset(): ");
  Serial.println(ok_reset ? "PASS" : "FAIL");
  if (!ok_reset) {
    overall_pass = false;
  }

  delay(20);

  bool ok_begin_after = sgp.begin();
  Serial.print("begin() after reset: ");
  Serial.println(ok_begin_after ? "PASS" : "FAIL");
  if (!ok_begin_after) {
    overall_pass = false;
  }

  uint16_t serial_after[3] = {0, 0, 0};
  bool ok_serial_after = sgp.getSerialNumber(serial_after);
  Serial.print("getSerialNumber (after): ");
  Serial.println(ok_serial_after ? "PASS" : "FAIL");
  if (ok_serial_after) {
    printSerial(serial_after);
  } else {
    overall_pass = false;
  }

  bool ok_serial_match = ok_serial_before && ok_serial_after &&
                         serialMatches(serial_before, serial_after);
  Serial.print("serial match: ");
  Serial.println(ok_serial_match ? "PASS" : "FAIL");
  if (!ok_serial_match) {
    overall_pass = false;
  }

  uint16_t sraw_voc = 0;
  bool ok_conditioning = sgp.executeConditioning(&sraw_voc);
  Serial.print("executeConditioning(): ");
  Serial.println(ok_conditioning ? "PASS" : "FAIL");
  if (!ok_conditioning) {
    overall_pass = false;
  }

  uint16_t sraw_voc_meas = 0;
  uint16_t sraw_nox_meas = 0;
  bool ok_measure = sgp.measureRawSignals(&sraw_voc_meas, &sraw_nox_meas);
  bool ok_values = ok_measure && (sraw_voc_meas > 0) && (sraw_nox_meas > 0);
  Serial.print("measureRawSignals(): ");
  Serial.println(ok_measure ? "PASS" : "FAIL");
  Serial.print("measurement values: ");
  Serial.println(ok_values ? "PASS" : "FAIL");
  if (ok_measure) {
    Serial.print("VOC: ");
    Serial.print(sraw_voc_meas);
    Serial.print(" NOx: ");
    Serial.println(sraw_nox_meas);
  }
  if (!ok_measure || !ok_values) {
    overall_pass = false;
  }

  Serial.print("TEST 11: ");
  Serial.println(overall_pass ? "PASS" : "FAIL");
}

void loop(void) {
  delay(1000);
}
