# Paqett SDK E2E Test Suite — Status

## Results: 44/44 passing

```
 ✓ WiFi & Network (4/4)
 ✓ Certificate Storage (6/6)
 ✓ Two-Step Provisioning (4/4)
 ✓ MQTT & Telemetry (7/7) — includes 50-msg burst + near-buffer-limit
 ✓ Shadow State (6/6) — includes rapid concurrent changes
 ✓ Commands - direct MQTT (4/4)
 ✓ Commands - server API (6/6) — includes TTL expiry
 ✓ MQTT Resilience (1/1) — forced disconnect recovery
 ✓ Edge Cases (2/2)
 ✓ OTA Updates (3/3) — bad SHA256 rejection + full update cycle
 ✓ Cleanup (1/1)
```

## How to run

```bash
# Prerequisites
cd paqett-server
PUBLIC_BASE_URL="http://192.168.1.240:3000" MQTT_PUBLIC_ENDPOINT="mqtts://192.168.1.240" npx tsx --no-cache src/main.ts

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
- Telemetry publishing (single, multi-field, rapid 10x, burst 50x, large payload, near-buffer-limit)
- Telemetry verified in server via REST API
- Shadow reported state (device → server)
- Shadow desired state (server → device via delta)
- Rapid concurrent shadow changes without corruption
- Commands via direct MQTT (calibrate, params, failure, unknown)
- Commands via server REST API (send, history, TTL, unknown, 404)
- Command TTL expiration tracking in server DB
- MQTT forced disconnect recovery (EMQX kick → reconnect → commands work)
- OTA bad SHA256 rejection (device doesn't crash)
- Full OTA update cycle (build v9.9.9 → upload → rollout → device downloads → flashes → reboots → reports new version → convergence)
- Device reset clears NVS

## Bugs found and fixed during testing

1. **Dangling `_instance` pointer** — `PaqettDevice` constructor unconditionally set `_instance = this`. When `testEdgeCases()` created temporary devices on the stack, `_instance` pointed to freed memory. Every MQTT callback after that dereferenced garbage → Guru Meditation crash (`LoadProhibited`). **Fix**: `if (_instance == nullptr) _instance = this`.

2. **Server `action` vs SDK `command` field** — Server published `{id, action, payload}` but device SDK read `doc["command"]`. Commands silently failed (nullptr early return). **Fix**: Server now sends `{id, command: action, payload}`.

3. **QoS 0 subscriptions** — Both shadow and command subscriptions used QoS 0 (default). Non-retained messages could be silently dropped. **Fix**: Upgraded to QoS 1 with return value checks.

4. **OTA version loop** — Device didn't check if `fw_target` matched current firmware version. After OTA reboot, retained delta re-triggered OTA infinitely. **Fix**: `strcmp(fwTarget, PAQETT_FIRMWARE_VERSION) != 0` guard.

5. **Serial `waitFor` event leak** — When `waitForEvent()` used the listener path (event arrives after wait starts), the consumed event stayed in the buffer. Next test picked it up → off-by-one failures. **Fix**: Listener callback now removes event from `this.lines`.

6. **EMQX CRL check** — `enable_crl_check = true` blocked ALL mTLS connections because step-ca CRL endpoints aren't reachable from Docker. **Fix**: Set to `false`.

7. **EMQX server cert missing LAN IP** — SAN only had `DNS:emqx, DNS:localhost, IP:127.0.0.1`. ESP32's mbedtls rejected when connecting to `192.168.1.240`. **Fix**: Regenerated cert with `--san 192.168.1.240`.

8. **Server returned wrong CA chain** — Provisioner returned only root_ca.crt but EMQX server cert is signed by intermediate CA. **Fix**: Return intermediate_ca.crt.

9. **PaqettCertStorage double-init** — ESP32 Preferences `begin()` fails if namespace already open. **Fix**: Added `close()` method.

10. **PubSubClient SUBACK timing** — `subscribe()` called in `begin()` but `loop()` not called before entering test phases. **Fix**: Added 10x loop() calls after subscribe.

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
- Interactive tests (shadow, commands, OTA) wait for host to trigger
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
| `device/src/PaqettDevice.cpp` | `_instance` guard, OTA version check, QoS 1, debug logging |
| `device/src/PaqettCommands.cpp` | QoS 1 subscribe with return check |
| `device/src/PaqettShadow.cpp` | QoS 1 subscribe with return check |
| `device/src/PaqettCertStorage.h/cpp` | Added `close()` method |
| `device/src/PaqettOTA.cpp` | Added `#include <WiFi.h>` |
| `device/src/PaqettWiFi.cpp` | Reordered cert/key setup |

## Server changes made during testing

| File | Change |
|------|--------|
| `emqx/emqx.conf` | `enable_crl_check = false` |
| `emqx/acl.conf` | Added `paqett-test-runner` ACL |
| `src/commands/command-service.ts` | `action` → `command: action` in MQTT payload |
| `src/auth/step-ca-provisioner.ts` | Returns intermediate CA as chain |
| `pki/emqx/server.crt` | Regenerated with LAN IP SAN |
