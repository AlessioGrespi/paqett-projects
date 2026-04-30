/**
 * @file 09_tick_conversion.ino
 *
 * Arduino hardware test for the Sensirion SGP41 tick conversion helpers.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp;

static bool checkTicks(const char *label, uint16_t expected, uint16_t actual,
                       uint16_t tolerance = 1) {
  uint16_t diff = (expected > actual) ? (expected - actual) : (actual - expected);
  bool pass = diff <= tolerance;
  Serial.print(label);
  Serial.print(" expected=");
  Serial.print(expected);
  Serial.print(" actual=");
  Serial.print(actual);
  Serial.print(" tol=");
  Serial.print(tolerance);
  Serial.print(" -> ");
  Serial.println(pass ? "PASS" : "FAIL");
  return pass;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 09: tick_conversion");

  if (!sgp.begin()) {
    Serial.println("SGP41 begin failed (ok for math-only test)");
  }

  bool all_pass = true;

  all_pass &= checkTicks("humidity 0%", 0, Adafruit_SGP41::humidityToTicks(0.0));
  all_pass &=
      checkTicks("humidity 50%", 32768, Adafruit_SGP41::humidityToTicks(50.0));
  all_pass &= checkTicks("humidity 100%", 65535,
                         Adafruit_SGP41::humidityToTicks(100.0));
  all_pass &= checkTicks("humidity -10% clamp", 0,
                         Adafruit_SGP41::humidityToTicks(-10.0));
  all_pass &= checkTicks("humidity 150% clamp", 65535,
                         Adafruit_SGP41::humidityToTicks(150.0));

  all_pass &= checkTicks("temp -45C", 0,
                         Adafruit_SGP41::temperatureToTicks(-45.0));
  all_pass &= checkTicks("temp 25C", 26214,
                         Adafruit_SGP41::temperatureToTicks(25.0));
  all_pass &= checkTicks("temp 130C", 65535,
                         Adafruit_SGP41::temperatureToTicks(130.0));
  all_pass &= checkTicks("temp -60C clamp", 0,
                         Adafruit_SGP41::temperatureToTicks(-60.0));
  all_pass &= checkTicks("temp 200C clamp", 65535,
                         Adafruit_SGP41::temperatureToTicks(200.0));

  if (all_pass) {
    Serial.println("TEST 09: PASS");
  } else {
    Serial.println("TEST 09: FAIL");
  }
}

void loop() {
  delay(1000);
}
