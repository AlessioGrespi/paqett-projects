#include <Arduino.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_SGP41.h>
#include <Adafruit_BME680.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_MAX1704X.h>

#define SDA_PIN 19
#define SCL_PIN 18
#define STEMMA_PWR 20
#define SLEEP_US 5000000  // 5 seconds in microseconds

Adafruit_LSM6DSOX lsm;
Adafruit_SGP41 sgp;
Adafruit_BME680 bme;
SensirionI2cScd4x scd;
Adafruit_MAX17048 maxlipo;

bool lsm_ok = false;
bool sgp_ok = false;
bool bme_ok = false;
bool scd_ok = false;
bool batt_ok = false;

// SGP41 needs ~10 seconds of conditioning before real readings
bool sgp_conditioned = false;
unsigned long sgp_start_time = 0;
const unsigned long SGP_CONDITIONING_MS = 10000;

const unsigned long READING_INTERVAL_MS = 10000;
unsigned long last_reading_ms = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Enable power to STEMMA QT connector
  pinMode(STEMMA_PWR, OUTPUT);
  digitalWrite(STEMMA_PWR, HIGH);
  delay(100);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println("\n========================================");
  Serial.println("  Atmospheric Sensing Station");
  Serial.println("  Adafruit Feather ESP32-C6");
  Serial.println("========================================\n");
  Serial.println("Initializing sensors...\n");

  // --- LSM6DSOX (IMU) ---
  if (lsm.begin_I2C(LSM6DS_I2CADDR_DEFAULT, &Wire)) {
    lsm_ok = true;
    lsm.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
    lsm.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
    lsm.setAccelDataRate(LSM6DS_RATE_26_HZ);
    lsm.setGyroDataRate(LSM6DS_RATE_26_HZ);
    Serial.println("[OK] LSM6DSOX (IMU)");
  } else {
    Serial.println("[FAIL] LSM6DSOX not found");
  }

  // --- BME690 (environment) ---
  if (bme.begin(0x77, &Wire) || bme.begin(0x76, &Wire)) {
    bme_ok = true;
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("[OK] BME690 (Temp/Hum/Press/Gas)");
  } else {
    Serial.println("[FAIL] BME690 not found at 0x77 or 0x76");
  }

  // --- SGP41 (VOC/NOx) ---
  if (sgp.begin(SGP41_DEFAULT_ADDR, &Wire)) {
    sgp_ok = true;
    sgp_start_time = millis();
    Serial.println("[OK] SGP41 (VOC/NOx) - conditioning...");
  } else {
    Serial.println("[FAIL] SGP41 not found");
  }

  // --- MAX17048 (Battery) ---
  if (maxlipo.begin(&Wire)) {
    batt_ok = true;
    maxlipo.quickStart();  // Force immediate SOC re-estimation from current voltage
    delay(500);
    Serial.printf("[OK] MAX17048 (Battery) — %.2fV, %.1f%%\n",
                  maxlipo.cellVoltage(), maxlipo.cellPercent());
  } else {
    Serial.println("[FAIL] MAX17048 not found at 0x36");
  }

  // --- SCD41 (CO2) ---
  scd.begin(Wire, SCD41_I2C_ADDR_62);
  uint64_t scd_serial;
  if (scd.getSerialNumber(scd_serial) == 0) {
    scd_ok = true;
    scd.startPeriodicMeasurement();
    Serial.printf("[OK] SCD41 (CO2) serial: %llX\n", scd_serial);
  } else {
    Serial.println("[FAIL] SCD41 not found");
  }

  Serial.println("\n--- Starting measurements ---\n");
}

void loop() {
  if (millis() - last_reading_ms < READING_INTERVAL_MS) return;
  last_reading_ms = millis();

  // --- BME690 (Environment) ---
  // Read temp/humidity/pressure first with heater off for clean readings,
  // then do a second pass with the gas heater on.
  if (bme_ok) {
    bme.setGasHeater(0, 0);  // heater off
    if (bme.performReading()) {
      Serial.println("BME690 (Environment):");
      Serial.printf("  Temp:       %6.1f °C\n", bme.temperature);
      Serial.printf("  Humidity:   %6.1f %%\n", bme.humidity);
      Serial.printf("  Pressure:   %6.1f hPa\n", bme.pressure / 100.0);
    }
    bme.setGasHeater(320, 150);  // heater back on
    if (bme.performReading()) {
      Serial.printf("  Gas R:      %6.1f kOhm\n", bme.gas_resistance / 1000.0);
    }
  }

  // --- SGP41 (Air Quality) ---
  if (sgp_ok) {
    float rh = bme_ok ? bme.humidity : 50.0;
    float temp = bme_ok ? bme.temperature : 25.0;

    if (!sgp_conditioned && (millis() - sgp_start_time < SGP_CONDITIONING_MS)) {
      uint16_t sraw_voc;
      sgp.executeConditioning(&sraw_voc, rh, temp);
      Serial.printf("SGP41 (Air Quality): conditioning... (%lus remaining)\n",
                    (SGP_CONDITIONING_MS - (millis() - sgp_start_time)) / 1000);
    } else {
      sgp_conditioned = true;
      uint16_t sraw_voc, sraw_nox;
      if (sgp.measureRawSignals(&sraw_voc, &sraw_nox, rh, temp)) {
        Serial.println("SGP41 (Air Quality):");
        Serial.printf("  VOC raw:    %5u\n", sraw_voc);
        Serial.printf("  NOx raw:    %5u\n", sraw_nox);
      }
    }
  }

  // --- SCD41 (CO2) ---
  if (scd_ok) {
    bool dataReady = false;
    scd.getDataReadyStatus(dataReady);
    if (dataReady) {
      uint16_t co2;
      float scd_temp, scd_hum;
      if (scd.readMeasurement(co2, scd_temp, scd_hum) == 0) {
        Serial.println("SCD41 (CO2):");
        Serial.printf("  CO2:        %5u ppm\n", co2);
        Serial.printf("  Temp (comp): %5.1f °C\n", scd_temp);
        Serial.printf("  Hum (comp):  %5.1f %%\n", scd_hum);
      }
    }
  }

  // --- LSM6DSOX (IMU) ---
  if (lsm_ok) {
    sensors_event_t accel, gyro, temp;
    lsm.getEvent(&accel, &gyro, &temp);
    Serial.println("LSM6DSOX (IMU):");
    Serial.printf("  Accel:  X=%+7.2f  Y=%+7.2f  Z=%+7.2f m/s2\n",
                  accel.acceleration.x, accel.acceleration.y, accel.acceleration.z);
    Serial.printf("  Gyro:   X=%+7.3f  Y=%+7.3f  Z=%+7.3f rad/s\n",
                  gyro.gyro.x, gyro.gyro.y, gyro.gyro.z);
    Serial.printf("  Temp (comp): %5.1f °C\n", temp.temperature);
  }

  // --- MAX17048 (Battery) ---
  if (batt_ok) {
    float batt_v = maxlipo.cellVoltage();
    float batt_pct = maxlipo.cellPercent();
    float charge_rate = maxlipo.chargeRate();
    Serial.println("MAX17048 (Battery):");
    Serial.printf("  Voltage:    %5.2f V\n", batt_v);
    Serial.printf("  Level:      %5.1f %%\n", batt_pct);
    Serial.printf("  Rate:       %+5.1f %%/hr\n", charge_rate);
    Serial.printf("  Status:     %s\n", charge_rate > 0.1 ? "Charging (USB)" : "On battery");
  }

  Serial.println("----------------------------------------");
}
