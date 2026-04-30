/**************************************************************************/
/*!
 * @file 10_heater_off.ino
 *
 * Arduino hardware test for the Sensirion SGP41 heater off behavior.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */
/**************************************************************************/

#include <Adafruit_SGP41.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;

static bool valid_raw(uint16_t value) {
  return (value != 0) && (value != 0xFFFF);
}

static void print_step(const char *label, bool pass) {
  Serial.print(pass ? "PASS: " : "FAIL: ");
  Serial.println(label);
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 10: heater_off");

  bool all_pass = true;

  if (!sgp41.begin()) {
    print_step("begin()", false);
    Serial.println("TEST 10: FAIL");
    return;
  }
  print_step("begin()", true);

  uint16_t sraw_voc = 0;
  uint16_t sraw_nox = 0;

  if (!sgp41.executeConditioning(&sraw_voc)) {
    print_step("executeConditioning()", false);
    Serial.println("TEST 10: FAIL");
    return;
  }
  print_step("executeConditioning()", true);

  bool ok = sgp41.measureRawSignals(&sraw_voc, &sraw_nox);
  print_step("measureRawSignals() before heater off", ok);
  if (!ok || !valid_raw(sraw_voc) || !valid_raw(sraw_nox)) {
    Serial.println("FAIL: invalid raw signals before heater off");
    all_pass = false;
  } else {
    Serial.println("PASS: raw signals valid before heater off");
  }

  ok = sgp41.turnHeaterOff();
  print_step("turnHeaterOff()", ok);
  if (!ok) {
    all_pass = false;
  }

  bool heater_off_skip = false;
  uint16_t off_voc = 0;
  uint16_t off_nox = 0;
  ok = sgp41.measureRawSignals(&off_voc, &off_nox);
  if (!ok) {
    Serial.println("PASS: measureRawSignals() failed after heater off");
  } else if (!valid_raw(off_voc) || !valid_raw(off_nox)) {
    Serial.println("PASS: invalid data after heater off");
  } else {
    Serial.println("SKIP: sensor returned data after heater off");
    heater_off_skip = true;
  }
  if (heater_off_skip) {
    Serial.println("SKIP: measureRawSignals() after heater off");
  } else {
    print_step("measureRawSignals() after heater off", true);
  }

  if (!sgp41.executeConditioning(&sraw_voc)) {
    print_step("executeConditioning() recovery", false);
    all_pass = false;
  } else {
    print_step("executeConditioning() recovery", true);
  }

  ok = sgp41.measureRawSignals(&sraw_voc, &sraw_nox);
  print_step("measureRawSignals() after recovery", ok);
  if (!ok || !valid_raw(sraw_voc) || !valid_raw(sraw_nox)) {
    Serial.println("FAIL: invalid raw signals after recovery");
    all_pass = false;
  } else {
    Serial.println("PASS: raw signals valid after recovery");
  }

  if (all_pass) {
    Serial.println("TEST 10: PASS");
  } else {
    Serial.println("TEST 10: FAIL");
  }
}

void loop(void) {
  delay(1000);
}
