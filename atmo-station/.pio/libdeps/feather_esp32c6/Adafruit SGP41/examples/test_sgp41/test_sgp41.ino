// Basic test for the Adafruit SGP41 sensor

#include <Adafruit_SGP41.h>

Adafruit_SGP41 sgp41;

void setup(void) {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println(F("Adafruit SGP41 test"));

  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41! Halting."));
    while (1) {
      delay(10);
    }
  }

  Serial.println(F("SGP41 found"));

  // Perform a soft reset
  Serial.println(F("Performing soft reset..."));
  if (!sgp41.softReset()) {
    Serial.println(F("Soft reset failed! Halting."));
    while (1) {
      delay(10);
    }
  }
  Serial.println(F("Soft reset successful."));
  delay(20); // Give it a moment to reset and come back to idle mode

  // Re-initialize after reset
  if (!sgp41.begin()) {
    Serial.println(F("Could not find SGP41 after reset! Halting."));
    while (1) {
      delay(10);
    }
  }

  uint16_t serial_number[3];
  if (sgp41.getSerialNumber(serial_number)) {
    Serial.print(F("Serial Number: 0x"));
    for (int i = 0; i < 3; i++) {
      if (serial_number[i] < 0x1000) Serial.print(F("0"));
      if (serial_number[i] < 0x100) Serial.print(F("0"));
      if (serial_number[i] < 0x10) Serial.print(F("0"));
      Serial.print(serial_number[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println(F("Failed to read serial number! Halting."));
    while (1) {
      delay(10);
    }
  }

  // Self Test
  Serial.println(F("Executing self test..."));
  uint16_t self_test_result = sgp41.executeSelfTest();
  if (self_test_result == SGP41_SELF_TEST_OK) {
    Serial.println(F("Self test passed!"));
  } else {
    Serial.print(F("Self test failed (0x"));
    Serial.print(self_test_result, HEX);
    Serial.print(F("): "));
    if (self_test_result & SGP41_SELF_TEST_VOC_FAIL_MASK) {
      Serial.print(F("VOC pixel failed! "));
    }
    if (self_test_result & SGP41_SELF_TEST_NOX_FAIL_MASK) {
      Serial.print(F("NOx pixel failed! "));
    }
    if (!((self_test_result & SGP41_SELF_TEST_VOC_FAIL_MASK) || (self_test_result & SGP41_SELF_TEST_NOX_FAIL_MASK))) {
        Serial.print(F("Unknown failure. "));
    }
    Serial.println(F("Halting."));
    while (1) {
        delay(10);
    }
  }
  
  // Execute conditioning
  Serial.println(F("Executing conditioning..."));
  uint16_t sraw_voc;
  if (sgp41.executeConditioning(&sraw_voc)) {
    Serial.print(F("Conditioning successful, SRAW_VOC: 0x"));
    Serial.println(sraw_voc, HEX);
  } else {
    Serial.println(F("Conditioning failed! Halting."));
    while (1) {
      delay(10);
    }
  }

  Serial.println(F("Waiting 7 seconds before starting measurements..."));
  delay(7000); // Wait 7 seconds after conditioning

  Serial.println(F("Setup complete. Starting measurement loop."));
}

void loop(void) {
  uint16_t sraw_voc = 0; 
  uint16_t sraw_nox = 0; 

  // Measure with default 50% RH and 25 degrees C compensation
  // Uses a non-blocking delay pattern, but delay(1000) at end makes it 1s intervals.
  if (sgp41.measureRawSignals(&sraw_voc, &sraw_nox)) {
    Serial.print(F("Raw VOC: "));
    Serial.print(sraw_voc, DEC);
    Serial.print(F("\tRaw NOx: "));
    Serial.println(sraw_nox, DEC);
  } else {
    Serial.println(F("Measurement failed!"));
  }
  delay(1000); // 1 second gap between measurements
}
