/**
 * Paqett Atmospheric Sensing Station
 * Adafruit Feather ESP32-C6 + STEMMA QT sensors
 *
 * Publishes sensor telemetry every 10 seconds via Paqett SDK.
 * Accepts commands to control the onboard NeoPixel LED.
 *
 * Sensors: BME680, SGP41, SCD41, LSM6DSOX, MAX17048
 * LED: Onboard NeoPixel (WS2812) on GPIO 8
 */

#include <Arduino.h>
#include <Wire.h>
#include <Paqett.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_SGP41.h>
#include <Adafruit_BME680.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_MAX1704X.h>

// ── Pin definitions ──────────────────────────────────────────

#define NEOPIXEL_PIN 9
#define SDA_PIN 19
#define SCL_PIN 18
#define STEMMA_PWR 20

// ── Globals ──────────────────────────────────────────────────

PaqettWiFi wifi;
PaqettDevice device;
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_LSM6DSOX lsm;
Adafruit_SGP41 sgp;
Adafruit_BME680 bme;
SensirionI2cScd4x scd;
Adafruit_MAX17048 maxlipo;

bool lsm_ok = false, sgp_ok = false, bme_ok = false, scd_ok = false, batt_ok = false;

bool sgp_conditioned = false;
unsigned long sgp_start_time = 0;
const unsigned long SGP_CONDITIONING_MS = 10000;

const unsigned long TELEMETRY_INTERVAL_MS = 10000;
unsigned long last_telemetry_ms = 0;

// Latest BME readings (shared with SGP41 for compensation)
float last_temp = 25.0, last_hum = 50.0;

// ── Sensor init ──────────────────────────────────────────────

void initSensors() {
  // Enable STEMMA QT power
  pinMode(STEMMA_PWR, OUTPUT);
  digitalWrite(STEMMA_PWR, HIGH);
  delay(100);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println("\nInitializing sensors...\n");

  // LSM6DSOX (IMU)
  if (lsm.begin_I2C(LSM6DS_I2CADDR_DEFAULT, &Wire)) {
    lsm_ok = true;
    lsm.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
    lsm.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
    lsm.setAccelDataRate(LSM6DS_RATE_26_HZ);
    lsm.setGyroDataRate(LSM6DS_RATE_26_HZ);
    Serial.println("[OK] LSM6DSOX (IMU)");
  } else {
    Serial.println("[--] LSM6DSOX not found");
  }

  // BME680 (Temp/Humidity/Pressure/Gas)
  if (bme.begin(0x77, &Wire) || bme.begin(0x76, &Wire)) {
    bme_ok = true;
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("[OK] BME680 (Temp/Hum/Press/Gas)");
  } else {
    Serial.println("[--] BME680 not found");
  }

  // SGP41 (VOC/NOx)
  if (sgp.begin(SGP41_DEFAULT_ADDR, &Wire)) {
    sgp_ok = true;
    sgp_start_time = millis();
    Serial.println("[OK] SGP41 (VOC/NOx) — conditioning...");
  } else {
    Serial.println("[--] SGP41 not found");
  }

  // MAX17048 (Battery fuel gauge)
  if (maxlipo.begin(&Wire)) {
    batt_ok = true;
    maxlipo.quickStart();
    delay(500);
    Serial.printf("[OK] MAX17048 (Battery) — %.2fV, %.1f%%\n",
                  maxlipo.cellVoltage(), maxlipo.cellPercent());
  } else {
    Serial.println("[--] MAX17048 not found");
  }

  // SCD41 (CO2)
  scd.begin(Wire, SCD41_I2C_ADDR_62);
  uint64_t scd_serial;
  if (scd.getSerialNumber(scd_serial) == 0) {
    scd_ok = true;
    scd.startPeriodicMeasurement();
    Serial.printf("[OK] SCD41 (CO2) serial: %llX\n", scd_serial);
  } else {
    Serial.println("[--] SCD41 not found");
  }
}

// ── LED helpers ──────────────────────────────────────────────

void setLed(int r, int g, int b, int brightness) {
  pixel.setBrightness(brightness);
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void ledOff() {
  pixel.clear();
  pixel.show();
}

// ── Read sensors and publish telemetry ───────────────────────

void publishSensorData() {
  JsonDocument doc;

  // BME680
  if (bme_ok) {
    bme.setGasHeater(0, 0);
    if (bme.performReading()) {
      last_temp = bme.temperature;
      last_hum = bme.humidity;
      doc["temperature"] = round(bme.temperature * 10) / 10.0;
      doc["humidity"] = round(bme.humidity * 10) / 10.0;
      doc["pressure"] = round(bme.pressure / 10.0) / 10.0; // hPa with 1 decimal
    }
    bme.setGasHeater(320, 150);
    if (bme.performReading()) {
      doc["gas_resistance"] = round(bme.gas_resistance / 100.0) / 10.0; // kOhm
    }
  }

  // SGP41
  if (sgp_ok) {
    if (!sgp_conditioned && (millis() - sgp_start_time < SGP_CONDITIONING_MS)) {
      uint16_t sraw_voc;
      sgp.executeConditioning(&sraw_voc, last_hum, last_temp);
    } else {
      sgp_conditioned = true;
      uint16_t sraw_voc, sraw_nox;
      if (sgp.measureRawSignals(&sraw_voc, &sraw_nox, last_hum, last_temp)) {
        doc["voc_raw"] = sraw_voc;
        doc["nox_raw"] = sraw_nox;
      }
    }
  }

  // SCD41
  if (scd_ok) {
    bool dataReady = false;
    scd.getDataReadyStatus(dataReady);
    if (dataReady) {
      uint16_t co2;
      float scd_temp, scd_hum;
      if (scd.readMeasurement(co2, scd_temp, scd_hum) == 0) {
        doc["co2"] = co2;
      }
    }
  }

  // LSM6DSOX
  if (lsm_ok) {
    sensors_event_t accel, gyro, temp;
    lsm.getEvent(&accel, &gyro, &temp);
    doc["accel_x"] = round(accel.acceleration.x * 100) / 100.0;
    doc["accel_y"] = round(accel.acceleration.y * 100) / 100.0;
    doc["accel_z"] = round(accel.acceleration.z * 100) / 100.0;
  }

  // Battery
  if (batt_ok) {
    doc["battery_voltage"] = round(maxlipo.cellVoltage() * 100) / 100.0;
    doc["battery_percent"] = round(maxlipo.cellPercent() * 10) / 10.0;
    doc["battery_charging"] = maxlipo.chargeRate() > 0.1;
  }

  bool ok = device.publishTelemetry(doc);

  #if PAQETT_DEBUG
  if (ok) {
    Serial.print("[telemetry] ");
    serializeJson(doc, Serial);
    Serial.println();
  } else {
    Serial.println("[telemetry] publish failed");
  }
  #endif
}

// ── Setup ────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n========================================");
  Serial.println("  Paqett Atmospheric Station v1.0.0");
  Serial.println("  Adafruit Feather ESP32-C6");
  Serial.println("========================================\n");

  // Enable NeoPixel + I2C power before anything else
  pinMode(STEMMA_PWR, OUTPUT);
  digitalWrite(STEMMA_PWR, HIGH);
  delay(50);

  // NeoPixel — brief blue flash to show we're alive
  pixel.begin();
  setLed(0, 0, 255, 30);

  // Sensors
  initSensors();

  // WiFi
  Serial.println("\nConnecting to WiFi...");
  wifi.connect(WIFI_SSID, WIFI_PASSWORD);
  if (!wifi.isConnected()) {
    Serial.println("[FAIL] WiFi connection failed");
    setLed(255, 0, 0, 30); // red = error
    return;
  }
  Serial.printf("[OK] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
  setLed(0, 255, 255, 20); // cyan = connecting to cloud

  // Paqett SDK
  device.setNetwork(&wifi);
  device.setApiUrl(API_URL);
  device.setApiKey(API_KEY);

  device.setStatusCallback([](PaqettStatus status) {
    const char* name = "unknown";
    switch (status) {
      case PAQETT_STATUS_PROVISIONING:   name = "provisioning"; break;
      case PAQETT_STATUS_MQTT_CONNECTING: name = "mqtt_connecting"; break;
      case PAQETT_STATUS_READY:          name = "ready"; break;
    }
    Serial.printf("[status] %s\n", name);
  });

  // Register LED command handlers
  device.onCommand("led_set", [](JsonObject& params) -> bool {
    JsonObject p = params["payload"].as<JsonObject>();
    int r = p["r"] | 0;
    int g = p["g"] | 0;
    int b = p["b"] | 0;
    int brightness = p["brightness"] | 50;
    setLed(r, g, b, brightness);
    Serial.printf("[led] set r=%d g=%d b=%d brightness=%d\n", r, g, b, brightness);
    return true;
  });

  device.onCommand("led_off", [](JsonObject& params) -> bool {
    ledOff();
    Serial.println("[led] off");
    return true;
  });

  // Shadow desired handler for persistent LED state (survives reboot)
  device.onDesired("led", [](const char* value) {
    // Format: "r,g,b,brightness" e.g. "255,0,0,50"
    int r = 0, g = 0, b = 0, brightness = 50;
    sscanf(value, "%d,%d,%d,%d", &r, &g, &b, &brightness);
    setLed(r, g, b, brightness);
    Serial.printf("[led] desired: r=%d g=%d b=%d brightness=%d\n", r, g, b, brightness);

    // Report back so delta clears
    JsonDocument state;
    state["led"] = value;
    device.reportState(state);
  });

  PaqettError err = device.begin();
  if (err != PAQETT_OK) {
    Serial.printf("[FAIL] Device init failed: %d\n", err);
    setLed(255, 0, 0, 30); // red = error
    return;
  }

  // Green = ready
  setLed(0, 255, 0, 20);
  delay(1000);
  ledOff();

  Serial.println("\n--- Ready. Publishing telemetry every 10s ---\n");
}

// ── Loop ─────────────────────────────────────────────────────

void loop() {
  device.loop();

  if (device.isReady() && millis() - last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
    last_telemetry_ms = millis();
    publishSensorData();
  }
}
