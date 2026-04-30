/**
 * @file gas_index.ino
 *
 * Example for the Adafruit SGP41 sensor with Sensirion Gas Index Algorithm.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>

Adafruit_SGP41 sgp41;
VOCGasIndexAlgorithm vocAlgorithm;
NOxGasIndexAlgorithm noxAlgorithm;

static void printSerialNumber(void) {
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

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println(F("Adafruit SGP41 Gas Index Algorithm test"));

  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41! Halting."));
    while (1) {
      delay(10);
    }
  }

  printSerialNumber();

  Serial.println(F("Note: VOC index needs ~60s and NOx index ~300s"));
  Serial.println(F("of learning before values become meaningful."));

  Serial.println(F("Conditioning (10 seconds)..."));
  for (uint8_t i = 0; i < 10; i++) {
    uint16_t sraw_voc = 0;
    if (sgp41.executeConditioning(&sraw_voc)) {
      int32_t voc_index = vocAlgorithm.process(sraw_voc);
      Serial.print(F("SRAW_VOC: "));
      Serial.print(sraw_voc);
      Serial.print(F("\tVOC Index: "));
      Serial.println(voc_index);
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
    int32_t voc_index = vocAlgorithm.process(sraw_voc);
    int32_t nox_index = noxAlgorithm.process(sraw_nox);

    Serial.print(F("Raw VOC: "));
    Serial.print(sraw_voc);
    Serial.print(F("\tRaw NOx: "));
    Serial.print(sraw_nox);
    Serial.print(F("\tVOC Index: "));
    Serial.print(voc_index);
    Serial.print(F("\tNOx Index: "));
    Serial.println(nox_index);
  } else {
    Serial.println(F("Measurement failed!"));
  }

  delay(1000);
}
