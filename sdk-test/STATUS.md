# Paqett SDK E2E Test Suite — Status

## Results: 30/39 passing

```
 ✓ WiFi & Network (4/4)
 ✓ Certificate Storage (6/6)
 ✓ Two-Step Provisioning (4/4)
 ✓ MQTT & Telemetry (5/5)
 ~ Shadow State (4/5) — 1 timing issue
 ✗ Commands - direct MQTT (0/4)
 ~ Commands - server API (2/5) — 3 fail on delivery
 ✓ OTA Updates (2/2 + 1 todo)
 ✓ Edge Cases (3/3)
```

## How to run

```bash
# Prerequisites
cd paqett-server
MQTT_PUBLIC_ENDPOINT="mqtts://192.168.1.240" npx tsx --no-cache src/main.ts

# Run tests (ESP32-S3 plugged into COM port)
cd paqett-projects/sdk-test
pnpm test
```

The test runner auto-detects the serial port, sends a start command to the firmware, and runs all phases sequentially. No manual button presses needed when using the S3's COM port.

## What works end-to-end

- WiFi connect/disconnect/error handling
- NVS certificate storage (save, load, clear, persist across reboot)
- Two-step provisioning (claim cert → device cert → stored in NVS)
- mTLS MQTT connection to EMQX on port 8883
- Telemetry publishing (single, multi-field, rapid 10x, large payload)
- Telemetry verified in server via REST API
- Shadow reported state (device → server)
- Shadow desired state (server → device via delta)
- Device reset clears NVS
- OTA bad SHA256 rejection

## Where it's stuck: commands don't reach the device

The remaining 8 failures are all the same root issue: **the device can't receive MQTT messages on the `commands` topic**, even though shadow delta delivery works perfectly.

### What works vs what doesn't

| Feature | Publisher | Topic | Works? |
|---------|-----------|-------|--------|
| Shadow delta | `paqett-ingest` (server) | `devices/{id}/shadow/delta` | ✓ |
| Commands | `paqett-ingest` (server API) | `devices/{id}/commands` | ✗ |
| Commands | `paqett-test-runner` (test) | `devices/{id}/commands` | ✗ |

Both shadow and commands use the same EMQX broker, same device subscription mechanism (`PubSubClient.subscribe()`), and the device is confirmed connected + subscribed (verified via EMQX REST API during test).

### What I've verified

- EMQX shows the device subscribed to `devices/{id}/commands` (QoS 0)
- EMQX shows the device subscribed to `devices/{id}/shadow/delta` (QoS 0)
- Shadow delta IS delivered (tests pass)
- Publishing from `paqett-test-runner` to `devices/{id}/commands` works in isolation (manual Node.js test with QoS 1 + 500ms delay)
- ACL rules are correct (verified inside Docker container)
- The `sendCommand` server API stores in DB and publishes via `paqett-ingest`

### What I suspect but haven't confirmed

1. **Timing**: The commands test runs ~100s into the test. The device's `PubSubClient` subscription to `commands` may have silently dropped (QoS 0, no SUBACK confirmation). Shadow delta works because it's retained and published with QoS 1.

2. **PubSubClient buffer**: The retained shadow delta message may be consuming the PubSubClient buffer, preventing the commands message from being processed. PubSubClient has a 2048-byte buffer.

3. **Callback routing**: The SDK's `mqttCallback` routes messages by checking `topic.indexOf("/shadow/")` and `topic.indexOf("/commands")`. If a retained delta message arrives during the commands phase, it might be processed first and block subsequent messages.

### Next steps to try

1. **Add debug logging to PubSubClient callback** — print every incoming MQTT message in the firmware to see if commands arrive at the PubSubClient level
2. **Check if PubSubClient.subscribe() returns true** — add a return value check and emit it as a test event
3. **Try QoS 1 subscription** — `PubSubClient.subscribe(topic, 1)` ensures SUBACK
4. **Clear retained messages** — old retained delta messages may interfere

## Bugs found during testing

1. **EMQX CRL check** — `enable_crl_check = true` in emqx.conf blocked ALL mTLS connections because step-ca CRL endpoints aren't reachable from Docker. Fixed: set to `false`.

2. **EMQX server cert missing LAN IP** — SAN only had `DNS:emqx, DNS:localhost, IP:127.0.0.1`. ESP32's mbedtls correctly rejected it when connecting to `192.168.1.240`. Fixed: regenerated cert with `--san 192.168.1.240`.

3. **Server returned wrong CA chain** — Provisioner returned only root_ca.crt but EMQX server cert is signed by intermediate CA. ESP32 couldn't verify server cert. Fixed: return intermediate_ca.crt.

4. **tsx caching** — Server code changes don't take effect without `--no-cache` flag. The compiled JS is cached by tsx/esbuild.

5. **PaqettCertStorage::initialize() can't be called twice** — ESP32 Preferences `begin()` fails if namespace already open. Fixed: added `close()` method.

6. **PaqettOTA.cpp missing `#include <WiFi.h>`** — Build error on ESP32-C6. Fixed.

7. **PubSubClient SUBACK not processed** — `subscribe()` called in `begin()` but `loop()` not called before entering test phases. Fixed: added loop() calls after subscribe.

8. **Server API response format** — Tests expected flat objects but API wraps in `{data: ...}` and `setDesired` expects `{state: ...}` wrapper. Fixed in test helper.

9. **Server `action` vs SDK `command` field** — Server sends `{id, action, payload}` but SDK reads `doc["command"]`. Not yet confirmed in tests (blocked by delivery issue), but will surface once commands work.

## Architecture

```
ESP32 Firmware (C++)          Test Runner (TypeScript/vitest)
├── Runs SDK test phases      ├── Opens serial port
├── Prints JSON lines         ├── Parses JSON events
├── Waits for host commands   ├── Calls server REST API
└── Reports pass/fail         ├── Subscribes to MQTT
                              └── Asserts everything
```

- Firmware and test runner communicate via JSON lines over serial
- Firmware waits for `{"action":"start"}` before running (handshake)
- Self-contained tests (WiFi, NVS) run automatically
- Interactive tests (shadow, commands) wait for host to trigger
- Test runner can reboot device via `{"action":"reboot"}`

## Files

| Path | Purpose |
|------|---------|
| `sdk-test/firmware/src/main.cpp` | Test firmware |
| `sdk-test/firmware/platformio.ini` | ESP32-S3 build config |
| `sdk-test/src/tests/main.test.ts` | All test cases |
| `sdk-test/src/helpers/serial.ts` | Serial port + JSON line parser |
| `sdk-test/src/helpers/api.ts` | Server REST API client |
| `sdk-test/src/helpers/mqtt.ts` | MQTT client (paqett-test-runner) |
| `sdk-test/src/helpers/pio.ts` | PlatformIO CLI wrapper |

## SDK changes made during testing

| File | Change |
|------|--------|
| `device/src/PaqettCertStorage.h/cpp` | Added `close()` method |
| `device/src/PaqettOTA.cpp` | Added `#include <WiFi.h>` |
| `device/src/PaqettDevice.cpp` | Added SUBACK loop, `PAQETT_MQTT_NO_TLS` flag |
| `device/src/PaqettWiFi.cpp` | Reordered cert/key setup |

## Server changes made during testing

| File | Change |
|------|--------|
| `emqx/emqx.conf` | `enable_crl_check = false` |
| `emqx/acl.conf` | Added `paqett-test-runner` ACL |
| `src/auth/step-ca-provisioner.ts` | Returns intermediate CA as chain |
| `src/api/routes/claim.ts` | Passes chain path to provisioner |
| `src/api/routes/devices.ts` | Passes chain path to provisioner |
| `src/api/routes/commands.ts` | (unchanged, but tested) |
| `src/shadow/shadow-mqtt.ts` | Debug logging (can remove) |
| `pki/emqx/server.crt` | Regenerated with LAN IP SAN |
