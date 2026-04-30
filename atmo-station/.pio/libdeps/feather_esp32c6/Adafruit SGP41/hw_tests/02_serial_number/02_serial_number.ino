/**
 * @file 02_serial_number.ino
 *
 * Arduino hardware test for the Sensirion SGP41 serial number.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include <Adafruit_SGP41.h>
#include <Arduino.h>
#include <Wire.h>

Adafruit_SGP41 sgp41;

static void printSerialHex(const uint16_t serial_number[3]) {
  Serial.print("0x");
  if (serial_number[0] < 0x1000)
    Serial.print('0');
  if (serial_number[0] < 0x100)
    Serial.print('0');
  if (serial_number[0] < 0x10)
    Serial.print('0');
  Serial.print(serial_number[0], HEX);
  Serial.print(" 0x");
  if (serial_number[1] < 0x1000)
    Serial.print('0');
  if (serial_number[1] < 0x100)
    Serial.print('0');
  if (serial_number[1] < 0x10)
    Serial.print('0');
  Serial.print(serial_number[1], HEX);
  Serial.print(" 0x");
  if (serial_number[2] < 0x1000)
    Serial.print('0');
  if (serial_number[2] < 0x100)
    Serial.print('0');
  if (serial_number[2] < 0x10)
    Serial.print('0');
  Serial.println(serial_number[2], HEX);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("TEST 02: serial_number");

  bool overall_pass = true;

  if (!sgp41.begin()) {
    Serial.println("FAIL: begin");
    Serial.println("TEST 02: FAIL");
    while (1) {
      delay(1000);
    }
  }

  uint16_t serial_first[3] = {0, 0, 0};
  uint16_t serial_second[3] = {0, 0, 0};

  bool read_ok = sgp41.getSerialNumber(serial_first);
  Serial.print("getSerialNumber() first: ");
  Serial.println(read_ok ? "PASS" : "FAIL");
  overall_pass &= read_ok;

  if (read_ok) {
    Serial.print("Serial number: ");
    printSerialHex(serial_first);
  }

  bool not_all_zero =
      (serial_first[0] != 0x0000) || (serial_first[1] != 0x0000) ||
      (serial_first[2] != 0x0000);
  Serial.print("not all zeros: ");
  Serial.println(not_all_zero ? "PASS" : "FAIL");
  overall_pass &= not_all_zero;

  bool not_all_ffff =
      (serial_first[0] != 0xFFFF) || (serial_first[1] != 0xFFFF) ||
      (serial_first[2] != 0xFFFF);
  Serial.print("not all 0xFFFF: ");
  Serial.println(not_all_ffff ? "PASS" : "FAIL");
  overall_pass &= not_all_ffff;

  bool read_ok_second = sgp41.getSerialNumber(serial_second);
  Serial.print("getSerialNumber() second: ");
  Serial.println(read_ok_second ? "PASS" : "FAIL");
  overall_pass &= read_ok_second;

  bool match_second = (serial_first[0] == serial_second[0]) &&
                      (serial_first[1] == serial_second[1]) &&
                      (serial_first[2] == serial_second[2]);
  Serial.print("serial matches second read: ");
  Serial.println(match_second ? "PASS" : "FAIL");
  overall_pass &= match_second;

  Serial.println(overall_pass ? "TEST 02: PASS" : "TEST 02: FAIL");

  while (1) {
    delay(1000);
  }
}

void loop() {}
