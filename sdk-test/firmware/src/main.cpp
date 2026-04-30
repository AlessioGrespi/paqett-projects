/**
 * Paqett SDK Test Firmware
 *
 * Runs all SDK feature tests and reports results as JSON lines over serial.
 * Self-contained tests (WiFi, NVS) run immediately on boot.
 * Interactive tests (shadow, commands) wait for host commands via serial.
 *
 * JSON protocol:
 *   Firmware → Host:
 *     {"type":"test","id":"X.Y","name":"...","status":"pass|fail","error":"..."}
 *     {"type":"ready","phase":"..."}
 *     {"type":"event","name":"...","value":"..."}
 *     {"type":"waiting","for":"..."}
 *   Host → Firmware:
 *     {"action":"start_phase","phase":"..."}
 *     {"action":"reset"}
 */

#include <Paqett.h>
#include <ArduinoJson.h>

// ── Configuration ──────────────────────────────────────────────
// WiFi credentials — override via build flags if needed
#ifndef TEST_WIFI_SSID
#define TEST_WIFI_SSID "YourSSID"
#endif
#ifndef TEST_WIFI_PASSWORD
#define TEST_WIFI_PASSWORD "YourPassword"
#endif
#ifndef TEST_API_URL
#define TEST_API_URL "https://your-server.com"
#endif
#ifndef TEST_API_KEY
#define TEST_API_KEY "paq_key_test"
#endif

PaqettWiFi wifi;
PaqettDevice device;

// State for interactive phases
String currentPhase = "";
bool phaseActive = false;
String thingName = "";

// Telemetry counter for host to track
int telemetrySentCount = 0;

// ── JSON output helpers ────────────────────────────────────────

void emitTest(const char* id, const char* name, bool passed, const char* error = nullptr) {
  JsonDocument doc;
  doc["type"] = "test";
  doc["id"] = id;
  doc["name"] = name;
  doc["status"] = passed ? "pass" : "fail";
  if (error) doc["error"] = error;
  serializeJson(doc, Serial);
  Serial.println();
}

void emitReady(const char* phase) {
  JsonDocument doc;
  doc["type"] = "ready";
  doc["phase"] = phase;
  serializeJson(doc, Serial);
  Serial.println();
}

void emitEvent(const char* name, const char* value) {
  JsonDocument doc;
  doc["type"] = "event";
  doc["name"] = name;
  doc["value"] = value;
  serializeJson(doc, Serial);
  Serial.println();
}

void emitEventInt(const char* name, int value) {
  JsonDocument doc;
  doc["type"] = "event";
  doc["name"] = name;
  doc["value"] = value;
  serializeJson(doc, Serial);
  Serial.println();
}

void emitWaiting(const char* forWhat) {
  JsonDocument doc;
  doc["type"] = "waiting";
  doc["for"] = forWhat;
  serializeJson(doc, Serial);
  Serial.println();
}

// ── Phase 1: WiFi Tests ────────────────────────────────────────

void testWifi() {
  emitReady("wifi");

  // 1.1: Happy-path connect
  bool ok = wifi.connect(TEST_WIFI_SSID, TEST_WIFI_PASSWORD, 15000, 2);
  if (ok && wifi.isConnected()) {
    String ip = wifi.getIPAddress();
    emitTest("1.1", "wifi_connect", ip.length() > 0);
  } else {
    emitTest("1.1", "wifi_connect", false, wifi.getLastError());
  }

  // 1.2: Disconnect
  wifi.disconnect();
  delay(500);
  emitTest("1.2", "wifi_disconnect", !wifi.isConnected());

  // 1.3: Wrong password (short timeout to avoid waiting forever)
  bool badPass = wifi.connect(TEST_WIFI_SSID, "wrong_password_123", 5000, 1);
  emitTest("1.3", "wifi_wrong_password", !badPass, nullptr);

  // 1.4: Wrong SSID
  bool badSsid = wifi.connect("nonexistent_network_xyz", "pass", 5000, 1);
  emitTest("1.4", "wifi_wrong_ssid", !badSsid, nullptr);

  // Reconnect for subsequent tests
  wifi.connect(TEST_WIFI_SSID, TEST_WIFI_PASSWORD, 15000, 3);
}

// ── Phase 2: Certificate Storage Tests ─────────────────────────

void testCertStorage() {
  emitReady("cert_storage");

  // 2.1: Initialize
  bool initOk = PaqettCertStorage::initialize();
  emitTest("2.1", "cert_init", initOk);

  // 2.2: Clear first, then check empty
  PaqettCertStorage::clearCertificates();
  emitTest("2.2", "cert_empty_check", !PaqettCertStorage::hasDeviceCertificate());

  // 2.3: Save and load round-trip
  const char* testCert = "-----BEGIN CERTIFICATE-----\nTESTCERT\n-----END CERTIFICATE-----";
  const char* testKey = "-----BEGIN PRIVATE KEY-----\nTESTKEY\n-----END PRIVATE KEY-----";
  const char* testCA = "-----BEGIN CERTIFICATE-----\nTESTCA\n-----END CERTIFICATE-----";

  bool saved = PaqettCertStorage::saveCertificates(testCert, testKey, testCA);
  bool loaded = PaqettCertStorage::loadCertificates();
  bool certMatch = strcmp(PaqettCertStorage::getDeviceCertificate(), testCert) == 0;
  bool keyMatch = strcmp(PaqettCertStorage::getPrivateKey(), testKey) == 0;
  bool caMatch = strcmp(PaqettCertStorage::getRootCA(), testCA) == 0;
  emitTest("2.3", "cert_save_load", saved && loaded && certMatch && keyMatch && caMatch);

  // 2.4: Thing name round-trip
  PaqettCertStorage::saveThingName("paq_TEST123");
  bool nameMatch = strcmp(PaqettCertStorage::getThingName(), "paq_TEST123") == 0;
  emitTest("2.4", "cert_thing_name", nameMatch);

  // 2.5: MQTT endpoint round-trip
  PaqettCertStorage::saveMqttEndpoint("mqtt.test.com");
  bool endpointMatch = strcmp(PaqettCertStorage::getMqttEndpoint(), "mqtt.test.com") == 0;
  emitTest("2.5", "cert_mqtt_endpoint", endpointMatch);

  // 2.6: Clear all
  PaqettCertStorage::clearCertificates();
  bool cleared = !PaqettCertStorage::hasDeviceCertificate();
  emitTest("2.6", "cert_clear", cleared);

  // Close the NVS preferences so device.begin() can re-initialize
  PaqettCertStorage::close();
}

// ── Phase 3: Provisioning (via device.begin()) ─────────────────

void testProvisioning() {
  emitReady("provisioning");

  // Certs already cleared and NVS closed by testCertStorage()

  // Configure device
  device.setNetwork(&wifi);
  device.setApiUrl(TEST_API_URL);
  device.setApiKey(TEST_API_KEY);

  // Status callback → emit events
  device.setStatusCallback([](PaqettStatus status) {
    const char* name = "unknown";
    switch (status) {
      case PAQETT_STATUS_CONNECTING:      name = "PAQETT_STATUS_CONNECTING"; break;
      case PAQETT_STATUS_PROVISIONING:    name = "PAQETT_STATUS_PROVISIONING"; break;
      case PAQETT_STATUS_MQTT_CONNECTING: name = "PAQETT_STATUS_MQTT_CONNECTING"; break;
      case PAQETT_STATUS_READY:           name = "PAQETT_STATUS_READY"; break;
      case PAQETT_STATUS_DISCONNECTED:    name = "PAQETT_STATUS_DISCONNECTED"; break;
      case PAQETT_STATUS_ERROR:           name = "PAQETT_STATUS_ERROR"; break;
    }
    emitEvent("status_changed", name);
  });

  // Register shadow desired handlers for Phase 5
  device.onDesired("valve", [](const char* value) {
    emitEvent("desired_received", "valve");
    // Report back
    JsonDocument state;
    state["valve"] = value;
    state["firmware"] = PAQETT_FIRMWARE_VERSION;
    device.reportState(state);
  });

  device.onDesired("interval", [](const char* value) {
    emitEvent("desired_received", "interval");
  });

  // Register command handlers for Phase 6
  device.onCommand("calibrate", [](JsonObject& params) -> bool {
    emitEvent("command_received", "calibrate");
    return true;
  });

  device.onCommand("blink", [](JsonObject& params) -> bool {
    int count = params["count"] | 3;
    JsonDocument doc;
    doc["type"] = "event";
    doc["name"] = "command_received";
    doc["value"] = "blink";
    doc["count"] = count;
    serializeJson(doc, Serial);
    Serial.println();
    return true;
  });

  device.onCommand("fail_test", [](JsonObject& params) -> bool {
    emitEvent("command_received", "fail_test");
    return false; // intentional failure
  });

  // Debug: log network state before begin
  Serial.print("[test] WiFi connected: ");
  Serial.println(wifi.isConnected() ? "yes" : "no");
  Serial.print("[test] API URL: ");
  Serial.println(TEST_API_URL);
  Serial.print("[test] API Key: ");
  Serial.println(String(TEST_API_KEY).substring(0, 20) + "...");

  // Run begin()
  PaqettError err = device.begin();
  bool provisioned = (err == PAQETT_OK);

  // Build error message with code
  char errMsg[64] = "";
  if (!provisioned) {
    const char* errNames[] = {"OK", "NETWORK_FAILED", "MQTT_FAILED", "PROVISIONING_FAILED", "CERT_STORAGE_FAILED", "NOT_INITIALIZED"};
    snprintf(errMsg, sizeof(errMsg), "begin() returned %s (%d)", errNames[err], err);
  }
  emitTest("3.1", "provision_first_boot", provisioned, provisioned ? nullptr : errMsg);

  if (provisioned) {
    thingName = device.getThingName();
    emitEvent("thing_name", thingName.c_str());

    // 3.2: isReady check
    emitTest("3.2", "device_is_ready", device.isReady());

    // 3.3: isConnected check
    emitTest("3.3", "device_is_connected", device.isConnected());
  }
}

// ── Phase 4: Telemetry (self-driven) ───────────────────────────

void testTelemetry() {
  emitReady("telemetry");

  if (!device.isReady()) {
    emitTest("4.1", "telemetry_publish", false, "device not ready");
    return;
  }

  // 4.1: Single telemetry publish
  {
    JsonDocument doc;
    doc["temperature"] = 22.5;
    doc["humidity"] = 60.0;
    doc["battery"] = 85;
    bool sent = device.publishTelemetry(doc);
    emitTest("4.1", "telemetry_publish", sent);
    if (sent) telemetrySentCount++;
  }

  // 4.2: Multiple fields
  {
    JsonDocument doc;
    doc["temperature"] = 23.1;
    doc["humidity"] = 58.5;
    doc["battery"] = 84;
    doc["pressure"] = 1013.25;
    doc["co2"] = 412;
    doc["voc"] = 150;
    bool sent = device.publishTelemetry(doc);
    emitTest("4.2", "telemetry_multi_field", sent);
    if (sent) telemetrySentCount++;
  }

  // 4.3: Rapid publish (10 messages)
  {
    int successCount = 0;
    for (int i = 0; i < 10; i++) {
      JsonDocument doc;
      doc["temperature"] = 20.0 + i * 0.5;
      doc["seq"] = i;
      if (device.publishTelemetry(doc)) {
        successCount++;
        telemetrySentCount++;
      }
      delay(50); // small gap to avoid flooding
    }
    emitTest("4.3", "telemetry_rapid_10", successCount == 10);
    emitEventInt("telemetry_sent", telemetrySentCount);
  }

  // 4.4: Large payload (~1500 bytes)
  {
    JsonDocument doc;
    for (int i = 0; i < 30; i++) {
      char key[16];
      snprintf(key, sizeof(key), "sensor_%02d", i);
      doc[key] = 100.0 + i * 1.1;
    }
    bool sent = device.publishTelemetry(doc);
    emitTest("4.4", "telemetry_large_payload", sent);
    if (sent) telemetrySentCount++;
  }

  // 4.5: Sustained burst (50 messages)
  {
    int successCount = 0;
    for (int i = 0; i < 50; i++) {
      JsonDocument doc;
      doc["seq"] = i;
      doc["temp"] = 20.0 + (i * 0.1);
      if (device.publishTelemetry(doc)) successCount++;
      delay(20);
    }
    emitTest("4.5", "telemetry_burst_50", successCount == 50);
  }

  // 4.6: Near buffer limit (~1900 bytes payload)
  {
    JsonDocument doc;
    // Create a payload close to the 2048-byte MQTT buffer
    for (int i = 0; i < 40; i++) {
      char key[16];
      snprintf(key, sizeof(key), "field_%02d", i);
      doc[key] = "abcdefghijklmnopqrstuvwxyz012345"; // 32-char value
    }
    bool sent = device.publishTelemetry(doc);
    emitTest("4.6", "telemetry_near_limit", sent);
  }
}

// ── Phase 8: Edge Cases ────────────────────────────────────────

void testEdgeCases() {
  emitReady("edge_cases");

  // 8.1: Publish before ready — create a fresh device (not initialized)
  {
    PaqettDevice tempDevice;
    JsonDocument doc;
    doc["test"] = true;
    bool sent = tempDevice.publishTelemetry(doc);
    emitTest("8.1", "publish_before_ready", !sent); // should return false
  }

  // 8.2: Loop before begin
  {
    PaqettDevice tempDevice;
    tempDevice.loop(); // should not crash
    emitTest("8.2", "loop_before_begin", true);
  }
}

// ── Serial command reader ──────────────────────────────────────

String serialBuffer = "";

bool checkSerialCommand(JsonDocument& cmd) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        DeserializationError err = deserializeJson(cmd, serialBuffer);
        serialBuffer = "";
        return err == DeserializationError::Ok;
      }
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
  return false;
}

// ── Main ───────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000); // wait for serial monitor

  emitReady("boot");

  // Wait for host to send {"action":"start"} before running tests.
  // This ensures the test runner is connected and listening.
  // Re-emit every second so the host catches it even if it connects late.
  {
    unsigned long lastEmit = 0;
    while (true) {
      if (millis() - lastEmit >= 1000) {
        emitWaiting("start");
        lastEmit = millis();
      }
      JsonDocument cmd;
      if (checkSerialCommand(cmd)) {
        const char* action = cmd["action"];
        if (action && strcmp(action, "start") == 0) break;
      }
      delay(50);
    }
  }

  // Phase 1: WiFi
  testWifi();

  // Phase 2: Certificate storage
  testCertStorage();

  // Phase 3: Provisioning (also registers shadow + command handlers)
  testProvisioning();

  // Phase 4: Telemetry
  testTelemetry();

  // Phase 8: Edge cases (runs independently)
  testEdgeCases();

  // Signal that self-driven tests are complete
  emitReady("self_tests_complete");

  // Now enter interactive mode for host-driven phases
  emitWaiting("host_command");
}

void loop() {
  // Keep MQTT alive
  device.loop();

  // Check for host commands
  JsonDocument cmd;
  if (checkSerialCommand(cmd)) {
    const char* action = cmd["action"];
    if (!action) return;

    if (strcmp(action, "start_phase") == 0) {
      const char* phase = cmd["phase"];
      if (!phase) return;

      if (strcmp(phase, "shadow") == 0) {
        // Phase 5: Shadow — host will push desired state via API
        // Handlers are already registered in testProvisioning()
        emitReady("shadow");
        // Report initial state
        JsonDocument state;
        state["valve"] = "open";
        state["firmware"] = PAQETT_FIRMWARE_VERSION;
        bool reported = device.reportState(state);
        emitTest("5.1", "shadow_report_state", reported);
        emitWaiting("shadow_desired");
      }
      else if (strcmp(phase, "commands") == 0) {
        // Phase 6: Commands — host will publish commands via MQTT
        // Handlers are already registered in testProvisioning()
        emitReady("commands");
        emitWaiting("command");
      }
    }
    else if (strcmp(action, "reset") == 0) {
      device.reset();
      bool cleared = !PaqettCertStorage::hasDeviceCertificate();
      emitTest("8.3", "device_reset", cleared);
      emitEvent("device_reset", "done");
    }
    else if (strcmp(action, "reboot") == 0) {
      Serial.println("[test] Rebooting...");
      delay(100);
      ESP.restart();
    }
  }
}
