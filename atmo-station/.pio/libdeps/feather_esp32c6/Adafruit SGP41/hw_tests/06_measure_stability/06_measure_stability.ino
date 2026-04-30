/**************************************************************************/
/*!
 * @file 06_measure_stability.ino
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

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 06: measure_stability");

  if (!sgp41.begin()) {
    Serial.println("FAIL: begin()");
    Serial.println("TEST 06: FAIL");
    return;
  }

  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (!sgp41.executeConditioning(&sraw_voc)) {
    Serial.println("FAIL: executeConditioning()");
    Serial.println("TEST 06: FAIL");
    return;
  }

  const uint8_t sample_count = 10;
  uint16_t voc_min = 0xFFFF;
  uint16_t voc_max = 0;
  uint16_t nox_min = 0xFFFF;
  uint16_t nox_max = 0;
  bool pass = true;

  for (uint8_t i = 0; i < sample_count; i++) {
    if (!sgp41.measureRawSignals(&sraw_voc, &sraw_nox)) {
      Serial.print("FAIL: measureRawSignals() #");
      Serial.println(i + 1);
      pass = false;
    }

    Serial.print("sraw_voc #");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(sraw_voc);

    Serial.print("sraw_nox #");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(sraw_nox);

    if (sraw_voc < voc_min) {
      voc_min = sraw_voc;
    }
    if (sraw_voc > voc_max) {
      voc_max = sraw_voc;
    }
    if (sraw_nox < nox_min) {
      nox_min = sraw_nox;
    }
    if (sraw_nox > nox_max) {
      nox_max = sraw_nox;
    }

    delay(1000);
  }

  uint16_t voc_spread = voc_max - voc_min;
  uint16_t nox_spread = nox_max - nox_min;

  Serial.print("VOC spread: ");
  Serial.println(voc_spread);
  Serial.print("NOx spread: ");
  Serial.println(nox_spread);

  if (voc_spread < 5000) {
    Serial.println("PASS: VOC stability");
  } else {
    Serial.println("FAIL: VOC stability");
    pass = false;
  }

  if (nox_spread < 5000) {
    Serial.println("PASS: NOx stability");
  } else {
    Serial.println("FAIL: NOx stability");
    pass = false;
  }

  if (pass) {
    Serial.println("TEST 06: PASS");
  } else {
    Serial.println("TEST 06: FAIL");
  }
}

void loop(void) {
  delay(1000);
}
