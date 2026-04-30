/**
 * @file simpletest.ino
 *
 * Simple test for the Adafruit SGP41 sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>

Adafruit_SGP41 sgp41;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println(F("Adafruit SGP41 simple test"));

  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41! Halting."));
    while (1) {
      delay(10);
    }
  }

  uint16_t serial_number[3];
  if (sgp41.getSerialNumber(serial_number)) {
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
  } else {
    Serial.println(F("Failed to read serial number! Halting."));
    while (1) {
      delay(10);
    }
  }

  Serial.println(F("Conditioning (10 seconds)..."));
  for (uint8_t i = 0; i < 10; i++) {
    uint16_t sraw_voc = 0;
    if (sgp41.executeConditioning(&sraw_voc)) {
      Serial.print(F("SRAW_VOC: "));
      Serial.println(sraw_voc);
    } else {
      Serial.println(F("Conditioning failed!"));
    }
    delay(1000);
  }

  Serial.println(F("Starting measurements."));
}

void loop(void) {
  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (sgp41.measureRawSignals(&sraw_voc, &sraw_nox)) {
    Serial.print(F("Raw VOC: "));
    Serial.print(sraw_voc);
    Serial.print(F("\tRaw NOx: "));
    Serial.println(sraw_nox);
  } else {
    Serial.println(F("Measurement failed!"));
  }

  delay(1000);
}
