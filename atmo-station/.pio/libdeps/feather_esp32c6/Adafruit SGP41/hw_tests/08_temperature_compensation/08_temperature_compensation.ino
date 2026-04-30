/**
 * @file 08_temperature_compensation.ino
 *
 * Arduino hardware test for the Sensirion SGP41 multi-pixel gas sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;

static float averageVoc(float temperature_c, float humidity_rh) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 3; i++) {
    uint16_t sraw_voc = 0;
    uint16_t sraw_nox = 0;
    if (!sgp41.measureRawSignals(&sraw_voc, &sraw_nox, humidity_rh,
                                 temperature_c)) {
      Serial.println("measureRawSignals failed");
      return 0;
    }
    sum += sraw_voc;
    delay(1000);
  }
  return sum / 3.0f;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 08: temperature_compensation");

  if (!sgp41.begin()) {
    Serial.println("begin failed");
    Serial.println("TEST 08: FAIL");
    return;
  }

  uint16_t conditioning_voc = 0;
  if (!sgp41.executeConditioning(&conditioning_voc)) {
    Serial.println("executeConditioning failed");
    Serial.println("TEST 08: FAIL");
    return;
  }

  float low_avg = averageVoc(0.0f, 50.0f);
  float high_avg = averageVoc(50.0f, 50.0f);

  Serial.print("Low temp avg sraw_voc: ");
  Serial.println(low_avg, 1);
  Serial.print("High temp avg sraw_voc: ");
  Serial.println(high_avg, 1);

  float diff = fabs(high_avg - low_avg);
  bool pass = diff >= 50.0f;

  Serial.print("Result: ");
  Serial.println(pass ? "PASS" : "FAIL");
  Serial.println(pass ? "TEST 08: PASS" : "TEST 08: FAIL");
}

void loop() {
  delay(1000);
}
