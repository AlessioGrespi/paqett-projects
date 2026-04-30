/**************************************************************************/
/*!
 * @file 07_humidity_compensation.ino
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

static bool take_average(float humidity, float temperature,
                         uint16_t *average) {
  uint32_t sum = 0;
  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  for (uint8_t i = 0; i < 3; i++) {
    delay(1000);
    if (!sgp41.measureRawSignals(&sraw_voc, &sraw_nox, humidity, temperature)) {
      Serial.print("FAIL: measureRawSignals() #");
      Serial.println(i + 1);
      return false;
    }
    Serial.print("sraw_voc #");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(sraw_voc);
    sum += sraw_voc;
  }

  *average = sum / 3;
  return true;
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 07: humidity_compensation");

  bool pass = true;

  if (!sgp41.begin()) {
    Serial.println("FAIL: begin()");
    Serial.println("TEST 07: FAIL");
    return;
  }

  uint16_t sraw_voc = 0;

  if (!sgp41.executeConditioning(&sraw_voc)) {
    Serial.println("FAIL: executeConditioning()");
    Serial.println("TEST 07: FAIL");
    return;
  }

  uint16_t low_avg = 0;
  uint16_t high_avg = 0;

  Serial.println("Low humidity (10% RH, 25C)");
  if (!take_average(10.0, 25.0, &low_avg)) {
    pass = false;
  }

  Serial.println("High humidity (90% RH, 25C)");
  if (!take_average(90.0, 25.0, &high_avg)) {
    pass = false;
  }

  Serial.print("low_avg: ");
  Serial.println(low_avg);
  Serial.print("high_avg: ");
  Serial.println(high_avg);

  uint16_t diff = (low_avg > high_avg) ? (low_avg - high_avg)
                                       : (high_avg - low_avg);
  Serial.print("difference: ");
  Serial.println(diff);

  if (diff >= 50) {
    Serial.println("PASS: humidity compensation shifted sraw_voc");
  } else {
    Serial.println("FAIL: humidity compensation shift < 50 ticks");
    pass = false;
  }

  if (pass) {
    Serial.println("TEST 07: PASS");
  } else {
    Serial.println("TEST 07: FAIL");
  }
}

void loop(void) {
  delay(1000);
}
