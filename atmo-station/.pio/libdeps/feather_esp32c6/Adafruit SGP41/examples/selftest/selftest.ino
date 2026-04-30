/**
 * @file selftest.ino
 *
 * Self-test and validation sketch for the Adafruit SGP41 sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>

Adafruit_SGP41 sgp41;

static void printSerialNumber(const uint16_t serial_number[3]) {
  Serial.print(F("Serial Number: 0x"));
  for (uint8_t i = 0; i < 3; i++) {
    if (serial_number[i] < 0x1000) {
      Serial.print(F("0"));
    }
    if (serial_number[i] < 0x100) {
      Serial.print(F("0"));
    }
    if (serial_number[i] < 0x10) {
      Serial.print(F("0"));
    }
    Serial.print(serial_number[i], HEX);
  }
  Serial.println();
}

static bool serialsMatch(const uint16_t a[3], const uint16_t b[3]) {
  for (uint8_t i = 0; i < 3; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println(F("Adafruit SGP41 self-test"));

  bool all_ok = true;

  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41!"));
    all_ok = false;
  }

  uint16_t serial_number[3] = {0, 0, 0};
  if (all_ok && sgp41.getSerialNumber(serial_number)) {
    printSerialNumber(serial_number);
  } else {
    Serial.println(F("Failed to read serial number."));
    all_ok = false;
  }

  if (all_ok) {
    Serial.println(F("Running self test..."));
    uint16_t self_test = sgp41.executeSelfTest();
    if (self_test == SGP41_SELF_TEST_OK) {
      Serial.println(F("Self test PASSED"));
    } else {
      all_ok = false;
      Serial.print(F("Self test FAILED: 0x"));
      Serial.println(self_test, HEX);
      if (self_test & SGP41_SELF_TEST_VOC_FAIL_MASK) {
        Serial.println(F("VOC pixel failed"));
      }
      if (self_test & SGP41_SELF_TEST_NOX_FAIL_MASK) {
        Serial.println(F("NOx pixel failed"));
      }
    }
  }

  if (all_ok) {
    Serial.println(F("Performing soft reset..."));
    if (!sgp41.softReset()) {
      Serial.println(F("Soft reset failed."));
      all_ok = false;
    }
  }

  if (all_ok) {
    delay(10);
    if (!sgp41.begin()) {
      Serial.println(F("Sensor did not respond after reset."));
      all_ok = false;
    }
  }

  uint16_t serial_number_after[3] = {0, 0, 0};
  if (all_ok && sgp41.getSerialNumber(serial_number_after)) {
    Serial.print(F("Serial after reset: 0x"));
    for (uint8_t i = 0; i < 3; i++) {
      if (serial_number_after[i] < 0x1000) {
        Serial.print(F("0"));
      }
      if (serial_number_after[i] < 0x100) {
        Serial.print(F("0"));
      }
      if (serial_number_after[i] < 0x10) {
        Serial.print(F("0"));
      }
      Serial.print(serial_number_after[i], HEX);
    }
    Serial.println();
  } else if (all_ok) {
    Serial.println(F("Failed to read serial number after reset."));
    all_ok = false;
  }

  if (all_ok && !serialsMatch(serial_number, serial_number_after)) {
    Serial.println(F("Serial number mismatch after reset."));
    all_ok = false;
  }

  if (all_ok) {
    Serial.println(F("All checks passed!"));
  } else {
    Serial.println(F("One or more checks failed."));
  }
}

void loop(void) {
  while (1) {
    delay(10);
  }
}
