/**
 * @file compensated_sht4x.ino
 *
 * Example for the Adafruit SGP41 sensor with SHT4x compensation and
 * Sensirion Gas Index Algorithm.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Adafruit_SHT4x.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>

Adafruit_SGP41 sgp41;
Adafruit_SHT4x sht4x;
VOCGasIndexAlgorithm vocAlgorithm;
NOxGasIndexAlgorithm noxAlgorithm;

static void printSGP41SerialNumber(void) {
  uint16_t serial_number[3];
  if (sgp41.getSerialNumber(serial_number)) {
    Serial.print(F("SGP41 Serial: 0x"));
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
    Serial.println(F("Failed to read SGP41 serial number! Halting."));
    while (1) {
      delay(10);
    }
  }
}

static void printSHT4xSerialNumber(void) {
  uint32_t serial = sht4x.readSerial();
  if (serial != 0xFFFFFFFF) {
    Serial.print(F("SHT4x Serial: 0x"));
    Serial.println(serial, HEX);
  } else {
    Serial.println(F("Failed to read SHT4x serial number! Halting."));
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

  Serial.println(F("Adafruit SGP41 + SHT4x compensated gas index test"));

  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41! Halting."));
    while (1) {
      delay(10);
    }
  }

  if (!sht4x.begin()) {
    Serial.println(F("Could not find SHT4x! Halting."));
    while (1) {
      delay(10);
    }
  }

  sht4x.setPrecision(SHT4X_HIGH_PRECISION);
  sht4x.setHeater(SHT4X_NO_HEATER);

  printSGP41SerialNumber();
  printSHT4xSerialNumber();

  Serial.println(F("Note: VOC index needs ~60s and NOx index ~300s"));
  Serial.println(F("of learning before values become meaningful."));

  Serial.println(F("Conditioning (10 seconds)..."));
  for (uint8_t i = 0; i < 10; i++) {
    sensors_event_t humidity_event;
    sensors_event_t temp_event;
    uint16_t sraw_voc = 0;

    if (!sht4x.getEvent(&humidity_event, &temp_event)) {
      Serial.println(F("SHT4x read failed!"));
    } else if (sgp41.executeConditioning(&sraw_voc,
                                         humidity_event.relative_humidity,
                                         temp_event.temperature)) {
      int32_t voc_index = vocAlgorithm.process(sraw_voc);
      Serial.print(F("T: "));
      Serial.print(temp_event.temperature, 1);
      Serial.print(F(" C\tRH: "));
      Serial.print(humidity_event.relative_humidity, 1);
      Serial.print(F(" %\tSRAW_VOC: "));
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
  sensors_event_t humidity_event;
  sensors_event_t temp_event;
  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (!sht4x.getEvent(&humidity_event, &temp_event)) {
    Serial.println(F("SHT4x read failed!"));
  } else if (sgp41.measureRawSignals(&sraw_voc, &sraw_nox,
                                     humidity_event.relative_humidity,
                                     temp_event.temperature)) {
    int32_t voc_index = vocAlgorithm.process(sraw_voc);
    int32_t nox_index = noxAlgorithm.process(sraw_nox);

    Serial.print(F("T: "));
    Serial.print(temp_event.temperature, 1);
    Serial.print(F(" C\tRH: "));
    Serial.print(humidity_event.relative_humidity, 1);
    Serial.print(F(" %\tRaw VOC: "));
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
