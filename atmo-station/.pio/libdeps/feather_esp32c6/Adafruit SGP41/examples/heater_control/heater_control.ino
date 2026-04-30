/**
 * @file heater_control.ino
 *
 * Demonstrate duty-cycling the SGP41 heater for low-power operation.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>

Adafruit_SGP41 sgp41;

static void runConditioning(uint8_t seconds) {
  Serial.print(F("Conditioning ("));
  Serial.print(seconds);
  Serial.println(F(" seconds)..."));

  for (uint8_t i = 0; i < seconds; i++) {
    uint16_t sraw_voc = 0;
    if (sgp41.executeConditioning(&sraw_voc)) {
      Serial.print(F("SRAW_VOC: "));
      Serial.println(sraw_voc);
    } else {
      Serial.println(F("Conditioning failed!"));
    }
    delay(1000);
  }
}

static void runMeasurements(uint8_t samples) {
  Serial.println(F("Starting measurements."));

  for (uint8_t i = 0; i < samples; i++) {
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
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println(F("Adafruit SGP41 heater control demo"));

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
}

void loop(void) {
  runConditioning(10);
  runMeasurements(5);

  if (sgp41.turnHeaterOff()) {
    Serial.println(F("Heater off — entering low power mode"));
  } else {
    Serial.println(F("Failed to turn heater off!"));
  }

  delay(5000);

  Serial.println(F("Waking up — re-conditioning sensor..."));
}
