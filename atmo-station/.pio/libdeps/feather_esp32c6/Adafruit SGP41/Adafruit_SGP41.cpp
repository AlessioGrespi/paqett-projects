/**
 * @file Adafruit_SGP41.cpp
 *
 * Arduino library for the Sensirion SGP41 multi-pixel gas sensor.
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code for
 * Adafruit Industries. MIT license, all text above must be included in any
 * redistribution.
 */

#include "Adafruit_SGP41.h"

/**
 * @brief Construct a new Adafruit_SGP41 instance.
 */
Adafruit_SGP41::Adafruit_SGP41(void) : _i2c_dev(nullptr), _wire(nullptr) {}

/**
 * @brief Destroy the Adafruit_SGP41 object and release resources.
 */
Adafruit_SGP41::~Adafruit_SGP41(void) {
  if (_i2c_dev) {
    delete _i2c_dev;
    _i2c_dev = nullptr;
  }
  _wire = nullptr;
}

/**
 * @brief Initialize I2C communication with the SGP41 sensor.
 * @param addr The I2C address to use.
 * @param wire Pointer to the TwoWire instance for I2C communication.
 * @return true if the device is detected and ready.
 * @return false if communication could not be established.
 */
bool Adafruit_SGP41::begin(uint8_t addr, TwoWire* wire) {
  _wire = wire;

  if (_i2c_dev) {
    delete _i2c_dev;
    _i2c_dev = nullptr;
  }

  _i2c_dev = new Adafruit_I2CDevice(addr, wire);
  if (!_i2c_dev) {
    return false;
  }

  if (!_i2c_dev->begin()) {
    delete _i2c_dev;
    _i2c_dev = nullptr;
    return false;
  }

  uint16_t serial_number[3];
  if (!getSerialNumber(serial_number)) {
    return false;
  }

  if ((serial_number[0] == serial_number[1]) &&
      (serial_number[1] == serial_number[2]) &&
      ((serial_number[0] == 0x0000) || (serial_number[0] == 0xFFFF))) {
    return false;
  }

  return true;
}

/**
 * @brief Execute the SGP41 conditioning command.
 * @param sraw_voc Pointer that will receive the VOC raw ticks.
 * @param default_rh Humidity in % (0-100).
 * @param default_t Temperature in degrees C (-45 to 130).
 * @return true on success, false otherwise.
 */
bool Adafruit_SGP41::executeConditioning(uint16_t* sraw_voc, float default_rh,
                                         float default_t) {
  if (!_i2c_dev || !sraw_voc) {
    return false;
  }

  uint16_t rh_ticks = humidityToTicks(default_rh);
  uint16_t t_ticks = temperatureToTicks(default_t);

  uint16_t payload[2] = {rh_ticks, t_ticks};
  if (!_writeCommand(SGP41_CMD_EXECUTE_CONDITIONING, payload, 2)) {
    return false;
  }

  delay(SGP41_CONDITIONING_DELAY_MS);

  return _readWords(sraw_voc, 1);
}

/**
 * @brief Measure raw VOC and NOx signals from the sensor.
 * @param sraw_voc Pointer that will receive the VOC raw ticks.
 * @param sraw_nox Pointer that will receive the NOx raw ticks.
 * @param relative_humidity Humidity in % (0-100).
 * @param temperature Temperature in degrees C (-45 to 130).
 * @return true on success, false otherwise.
 */
bool Adafruit_SGP41::measureRawSignals(uint16_t* sraw_voc, uint16_t* sraw_nox,
                                       float relative_humidity,
                                       float temperature) {
  if (!_i2c_dev || !sraw_voc || !sraw_nox) {
    return false;
  }

  uint16_t rh_ticks = humidityToTicks(relative_humidity);
  uint16_t t_ticks = temperatureToTicks(temperature);

  uint16_t payload[2] = {rh_ticks, t_ticks};
  if (!_writeCommand(SGP41_CMD_MEASURE_RAW_SIGNALS, payload, 2)) {
    return false;
  }

  delay(SGP41_MEASUREMENT_DELAY_MS);

  uint16_t results[2];
  if (!_readWords(results, 2)) {
    return false;
  }

  *sraw_voc = results[0];
  *sraw_nox = results[1];
  return true;
}

/**
 * @brief Run the built-in self-test sequence.
 * @return Raw 16-bit test result value, or 0 on failure.
 */
uint16_t Adafruit_SGP41::executeSelfTest(void) {
  if (!_i2c_dev) {
    return 0;
  }

  if (!_writeCommand(SGP41_CMD_EXECUTE_SELF_TEST)) {
    return 0;
  }

  delay(SGP41_SELF_TEST_DELAY_MS);

  uint16_t result = 0;
  if (!_readWords(&result, 1)) {
    return 0;
  }

  return result;
}

/**
 * @brief Turn off the integrated heater and enter idle mode.
 * @return true on success, false otherwise.
 */
bool Adafruit_SGP41::turnHeaterOff(void) {
  if (!_i2c_dev) {
    return false;
  }

  if (!_writeCommand(SGP41_CMD_TURN_HEATER_OFF)) {
    return false;
  }

  delay(1);
  return true;
}

/**
 * @brief Retrieve the 48-bit serial number from the sensor.
 * @param serial_number Array of three 16-bit words to store the serial number.
 * @return true on success, false otherwise.
 */
bool Adafruit_SGP41::getSerialNumber(uint16_t serial_number[3]) {
  if (!_i2c_dev || !serial_number) {
    return false;
  }

  if (!_writeCommand(SGP41_CMD_GET_SERIAL_NUMBER)) {
    return false;
  }

  delay(1);

  return _readWords(serial_number, 3);
}

/**
 * @brief Issue an I2C general call soft reset.
 * @return true on success, false otherwise.
 */
bool Adafruit_SGP41::softReset(void) {
  if (!_wire) {
    return false;
  }

  _wire->beginTransmission(SGP41_GENERAL_CALL_ADDR);
  _wire->write(SGP41_CMD_SOFT_RESET >> 8);
  _wire->write(SGP41_CMD_SOFT_RESET & 0xFF);
  _wire->endTransmission(); // No error check for general call

  delay(20); // Longer delay after reset for device to recover
  return true;
}

/**************************************************************************/
/*!
    @brief Convert relative humidity in % to the sensor's tick format.
    @param humidity Relative humidity in percent (0-100).
    @return Humidity value in ticks (0-65535).
*/
/**************************************************************************/
uint16_t Adafruit_SGP41::humidityToTicks(float humidity) {
  if (humidity < 0)
    humidity = 0;
  if (humidity > 100)
    humidity = 100;
  return (uint16_t)(humidity * 65535.0f / 100.0f + 0.5f);
}

/**************************************************************************/
/*!
    @brief Convert temperature in degrees Celsius to the sensor's tick format.
    @param temperature Temperature in degrees Celsius (-45 to 130).
    @return Temperature value in ticks (0-65535).
*/
/**************************************************************************/
uint16_t Adafruit_SGP41::temperatureToTicks(float temperature) {
  if (temperature < -45)
    temperature = -45;
  if (temperature > 130)
    temperature = 130;
  return (uint16_t)((temperature + 45.0f) * 65535.0f / 175.0f + 0.5f);
}

/**************************************************************************/
/*!
    @brief Write a command and optional payload words to the sensor.
    @param command 16-bit command code.
    @param data_words Pointer to 16-bit payload words (can be nullptr).
    @param num_words Number of payload words.
    @returns True on success.
*/
/**************************************************************************/
bool Adafruit_SGP41::_writeCommand(uint16_t command, const uint16_t* data_words,
                                   uint8_t num_words) {
  if (!_i2c_dev) {
    return false;
  }

  uint8_t buffer[2 + 3 * 3];
  uint8_t index = 0;
  buffer[index++] = command >> 8;
  buffer[index++] = command & 0xFF;

  for (uint8_t i = 0; i < num_words; i++) {
    uint16_t word = data_words[i];
    buffer[index++] = word >> 8;
    buffer[index++] = word & 0xFF;
    buffer[index++] = _crc8(word);
  }

  return _i2c_dev->write(buffer, index);
}

/**************************************************************************/
/*!
    @brief Read 16-bit data words from the sensor, verifying CRC.
    @param data Pointer to store the received words.
    @param num_words Number of 16-bit words expected.
    @returns True on success.
*/
/**************************************************************************/
bool Adafruit_SGP41::_readWords(uint16_t* data, uint8_t num_words) {
  if (!_i2c_dev) {
    return false;
  }

  if (num_words == 0) {
    return true;
  }

  uint8_t buffer[3 * 3];
  uint8_t read_len = num_words * 3;

  if (!_i2c_dev->read(buffer, read_len)) {
    return false;
  }

  for (uint8_t i = 0, j = 0; i < num_words; i++, j += 3) {
    uint16_t word = ((uint16_t)buffer[j] << 8) | buffer[j + 1];
    uint8_t crc = buffer[j + 2];
    if (_crc8(word) != crc) {
      return false;
    }
    data[i] = word;
  }

  return true;
}

/**************************************************************************/
/*!
    @brief Compute the CRC-8 checksum used by the SGP41.
    @param word 16-bit data word to protect.
    @returns The CRC-8 checksum value.
*/
/**************************************************************************/
uint8_t Adafruit_SGP41::_crc8(uint16_t word) {
  uint8_t crc = 0xFF;
  uint8_t bytes[2] = {static_cast<uint8_t>(word >> 8),
                      static_cast<uint8_t>(word & 0xFF)};

  for (uint8_t i = 0; i < 2; i++) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}
