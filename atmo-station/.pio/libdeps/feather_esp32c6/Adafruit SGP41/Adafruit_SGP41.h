/**
 * @file Adafruit_SGP41.h
 *
 * Arduino library for the Sensirion SGP41 multi-pixel gas sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#ifndef ADAFRUIT_SGP41_H
#define ADAFRUIT_SGP41_H

#include <Adafruit_I2CDevice.h>
#include <Arduino.h>
#include <Wire.h>

/** Default I2C address for the SGP41 sensor. */
#define SGP41_DEFAULT_ADDR 0x59

/** Command code for executing the SGP41 conditioning sequence. */
#define SGP41_CMD_EXECUTE_CONDITIONING 0x2612

/** Command code for measuring raw VOC and NOx signals. */
#define SGP41_CMD_MEASURE_RAW_SIGNALS 0x2619

/** Command code for starting the on-chip self-test. */
#define SGP41_CMD_EXECUTE_SELF_TEST 0x280E

/** Command code for turning the SGP4x heater off and entering idle. */
#define SGP41_CMD_TURN_HEATER_OFF 0x3615

/** Command code for reading the SGP4x serial number. */
#define SGP41_CMD_GET_SERIAL_NUMBER 0x3682

/** Command code for performing an I2C general call soft reset. */
#define SGP41_CMD_SOFT_RESET 0x0006

/** Default humidity word used to disable on-chip humidity compensation. */
#define SGP41_DEFAULT_HUMIDITY 0x8000

/** Default temperature word used to disable on-chip humidity compensation. */
#define SGP41_DEFAULT_TEMPERATURE 0x6666

/** Delay in milliseconds required after conditioning command. */
#define SGP41_CONDITIONING_DELAY_MS 50

/** Delay in milliseconds required after raw signal measurement command. */
#define SGP41_MEASUREMENT_DELAY_MS 50

/** Delay in milliseconds required after executing the self-test command. */
#define SGP41_SELF_TEST_DELAY_MS 320

/** I2C general call address for issuing a soft reset. */
#define SGP41_GENERAL_CALL_ADDR 0x00

/** Expected self-test pass value returned by the sensor. */
#define SGP41_SELF_TEST_OK 0xD400

/** Self-test bitmask for VOC pixel failure. */
#define SGP41_SELF_TEST_VOC_FAIL_MASK 0x0001
/** Self-test bitmask for NOx pixel failure. */
#define SGP41_SELF_TEST_NOX_FAIL_MASK 0x0002

/**************************************************************************/
/*!
    @brief Class for communicating with the Sensirion SGP41 gas sensor.
*/
/**************************************************************************/
class Adafruit_SGP41 {
 public:
  Adafruit_SGP41(void);
  ~Adafruit_SGP41(void);

  bool begin(uint8_t addr = SGP41_DEFAULT_ADDR, TwoWire* wire = &Wire);

  bool executeConditioning(uint16_t* sraw_voc, float default_rh = 50.0,
                           float default_t = 25.0);

  bool measureRawSignals(uint16_t* sraw_voc, uint16_t* sraw_nox,
                         float relative_humidity = 50.0,
                         float temperature = 25.0);

  uint16_t executeSelfTest(void);
  bool turnHeaterOff(void);
  bool getSerialNumber(uint16_t serial_number[3]);
  bool softReset(void);

  static uint16_t humidityToTicks(float humidity);
  static uint16_t temperatureToTicks(float temperature);

 private:
  bool _writeCommand(uint16_t command, const uint16_t* data_words = nullptr,
                     uint8_t num_words = 0);
  bool _readWords(uint16_t* data, uint8_t num_words);
  uint8_t _crc8(uint16_t word);

  /** I2C device interface pointer. */
  Adafruit_I2CDevice* _i2c_dev;
  /** Stored pointer to the active TwoWire bus (not accessed directly). */
  TwoWire* _wire;
};

#endif
