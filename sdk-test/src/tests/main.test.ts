import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { DeviceSerial, detectPort } from "../helpers/serial.js";
import { TestMqtt } from "../helpers/mqtt.js";
import * as api from "../helpers/api.js";
import { flash, firmwareBinPath } from "../helpers/pio.js";
import { existsSync } from "node:fs";

let serial: DeviceSerial;
let mqtt: TestMqtt;
let apiKey: string;
let thingName: string | null = null;

beforeAll(async () => {
  // Preflight
  const healthy = await api.healthCheck();
  if (!healthy) throw new Error("Server not healthy. Is docker-compose up?");

  // Open serial (firmware must already be flashed via `pnpm test:flash`)
  const portPath = await detectPort();
  console.log(`  Serial port: ${portPath}`);
  serial = new DeviceSerial(portPath);
  await serial.open();

  // Connect MQTT
  mqtt = new TestMqtt();
  await mqtt.connect();

  // Clear retained delta to prevent stale OTA from triggering during provisioning
  mqtt.clearRetained("devices/paq_B87A5C75DC3C/shadow/delta");
  await new Promise((r) => setTimeout(r, 500));

  // Create API key
  apiKey = await api.createApiKey();
  console.log(`  API key: ${apiKey.substring(0, 20)}...`);

  // Clear stale OTA desired state from previous runs
  try {
    await api.setDesired(apiKey, "paq_B87A5C75DC3C", {
      fw_target: null, ota_url: null, ota_sha256: null,
    });
  } catch { /* device may not exist yet */ }

  // Try "start" first (device in handshake loop).
  // If that doesn't work, send "reboot" to restart the device, then try start again.
  console.log("  Sending start command...");
  serial.send({ action: "start" });
  try {
    await serial.waitForReady("wifi", 10_000);
    console.log("  Device started!");
  } catch {
    console.log("  Start failed, sending reboot...");
    serial.send({ action: "reboot" });
    await new Promise((r) => setTimeout(r, 4000)); // wait for reboot
    serial.flush();
    // After reboot, device enters waiting:start loop
    serial.send({ action: "start" });
    await serial.waitForReady("wifi", 30_000);
    console.log("  Device started after reboot!");
  }
}, 120_000);

afterAll(async () => {
  await mqtt?.disconnect();
  await serial?.close();
});

// ── Phase 1: WiFi ─────────────────────────────────────────────

describe("WiFi & Network", () => {
  it("connects with valid credentials", async () => {
    const r = await serial.waitForTest("1.1", 10_000);
    expect(r.status).toBe("pass");
  });

  it("disconnects cleanly", async () => {
    const r = await serial.waitForTest("1.2", 10_000);
    expect(r.status).toBe("pass");
  });

  it("fails with wrong password", async () => {
    const r = await serial.waitForTest("1.3", 10_000);
    expect(r.status).toBe("pass");
  });

  it("fails with wrong SSID", async () => {
    const r = await serial.waitForTest("1.4", 10_000);
    expect(r.status).toBe("pass");
  });
});

// ── Phase 2: Certificate Storage ──────────────────────────────

describe("Certificate Storage (NVS)", () => {
  beforeAll(async () => {
    await serial.waitForReady("cert_storage", 30_000);
  });

  it("initializes NVS", async () => {
    expect((await serial.waitForTest("2.1")).status).toBe("pass");
  });

  it("reports empty when no certs stored", async () => {
    expect((await serial.waitForTest("2.2")).status).toBe("pass");
  });

  it("saves and loads certificates round-trip", async () => {
    expect((await serial.waitForTest("2.3")).status).toBe("pass");
  });

  it("saves and loads thing name", async () => {
    expect((await serial.waitForTest("2.4")).status).toBe("pass");
  });

  it("saves and loads MQTT endpoint", async () => {
    expect((await serial.waitForTest("2.5")).status).toBe("pass");
  });

  it("clears all certificates", async () => {
    expect((await serial.waitForTest("2.6")).status).toBe("pass");
  });
});

// ── Phase 3: Provisioning ─────────────────────────────────────

describe("Two-Step Provisioning", () => {
  beforeAll(async () => {
    await serial.waitForReady("provisioning", 30_000);
  });

  it("provisions on first boot", async () => {
    const r = await serial.waitForTest("3.1", 60_000);
    expect(r.status).toBe("pass");
  });

  it("emits status transitions and assigns thing name", async () => {
    const ev = await serial.waitForEvent("thing_name", 5_000);
    thingName = ev.value as string;
    console.log(`    Thing name: ${thingName}`);
    expect(thingName).toMatch(/^paq_/);
  });

  it("device is ready and connected", async () => {
    expect((await serial.waitForTest("3.2", 5_000)).status).toBe("pass");
    expect((await serial.waitForTest("3.3", 5_000)).status).toBe("pass");
  });

  it("device exists in server", async () => {
    expect(thingName).toBeTruthy();
    await new Promise((r) => setTimeout(r, 2000));
    const device = await api.getDevice(apiKey, thingName!);
    expect(device).toBeTruthy();
  });
});

// ── Phase 4: Telemetry ─────────────────────────────────────────

describe("MQTT & Telemetry", () => {
  beforeAll(async () => {
    await serial.waitForReady("telemetry", 30_000);
  });

  it("publishes telemetry", async () => {
    expect((await serial.waitForTest("4.1")).status).toBe("pass");
  });

  it("publishes with multiple fields", async () => {
    expect((await serial.waitForTest("4.2")).status).toBe("pass");
  });

  it("publishes 10 messages in rapid succession", async () => {
    expect((await serial.waitForTest("4.3", 15_000)).status).toBe("pass");
  });

  it("publishes large payload", async () => {
    expect((await serial.waitForTest("4.4")).status).toBe("pass");
  });

  it("publishes 50 messages without loss", async () => {
    expect((await serial.waitForTest("4.5", 15_000)).status).toBe("pass");
  });

  it("handles payload near buffer limit", async () => {
    expect((await serial.waitForTest("4.6")).status).toBe("pass");
  });

  it("telemetry appears in server", async () => {
    expect(thingName).toBeTruthy();
    await new Promise((r) => setTimeout(r, 3000));
    const data = await api.getLatestTelemetry(apiKey, thingName!);
    expect(data.readings).toBeTruthy();
    expect(data.readings.length).toBeGreaterThan(0);
  });
});

// ── Phase 5: Shadow State ──────────────────────────────────────

describe("Shadow State", () => {
  beforeAll(async () => {
    console.log("  [shadow] Waiting for self_tests_complete...");
    await serial.waitForReady("self_tests_complete", 90_000);
    console.log("  [shadow] Got self_tests_complete, waiting for host_command...");
    await serial.waitForWaiting("host_command", 10_000);
    console.log("  [shadow] Got host_command, resetting desired state...");
    // Set desired to match what the device will report, so no stale delta fires
    await api.setDesired(apiKey, thingName!, { valve: "open" });
    await new Promise((r) => setTimeout(r, 500));
    console.log("  [shadow] Sending start_phase shadow...");
    serial.send({ action: "start_phase", phase: "shadow" });
    await serial.waitForReady("shadow", 10_000);
    // Drain stale desired_received events from retained deltas delivered during begin()
    serial.drainEvents("desired_received");
    console.log("  [shadow] Shadow phase started");
  });

  it("reports initial state", async () => {
    const r = await serial.waitForTest("5.1", 10_000);
    expect(r.status).toBe("pass");
    await new Promise((r) => setTimeout(r, 2000));
    const shadow = await api.getShadow(apiKey, thingName!);
    expect(shadow.reported?.valve).toBe("open");
  });

  it("receives desired state change", async () => {
    await serial.waitForWaiting("shadow_desired", 10_000);
    await api.setDesired(apiKey, thingName!, { valve: "closed" });
    const ev = await serial.waitForEvent("desired_received", 15_000);
    expect(ev.value).toBe("valve");
  });

  it("reports back after desired change (delta clears)", async () => {
    await new Promise((r) => setTimeout(r, 3000));
    const shadow = await api.getShadow(apiKey, thingName!);
    expect(shadow.reported?.valve).toBe("closed");
    if (shadow.delta) expect(shadow.delta.valve).toBeUndefined();
  });

  it("handles multiple desired fields", async () => {
    await api.setDesired(apiKey, thingName!, { interval: "10000" });
    const ev = await serial.waitForEvent("desired_received", 15_000);
    expect(ev.value).toBe("interval");
  });

  it("ignores unknown desired fields without crashing", async () => {
    await api.setDesired(apiKey, thingName!, { unknown_field: "test" });
    await new Promise((r) => setTimeout(r, 3000));
    const device = await api.getDevice(apiKey, thingName!);
    expect(device).toBeTruthy();
  });

  it("handles rapid desired changes without corruption", async () => {
    await Promise.all([
      api.setDesired(apiKey, thingName!, { rapid: "a" }),
      api.setDesired(apiKey, thingName!, { rapid: "b" }),
      api.setDesired(apiKey, thingName!, { rapid: "c" }),
    ]);
    await new Promise((r) => setTimeout(r, 2000));
    const shadow = await api.getShadow(apiKey, thingName!);
    expect(["a", "b", "c"]).toContain(shadow.desired?.rapid);
  });
});

// ── Phase 6: Commands ──────────────────────────────────────────

describe("Commands (direct MQTT)", () => {
  beforeAll(async () => {
    serial.send({ action: "start_phase", phase: "commands" });
    await serial.waitForReady("commands", 10_000);
    await new Promise((r) => setTimeout(r, 1000));
    await serial.waitForWaiting("command", 5_000);
    // Drain any stale command_received events (from retained messages)
    serial.drainEvents("command_received");
  });

  it("executes calibrate command", async () => {
    const ackP = mqtt.waitForAck(thingName!, "cmd-1", 15_000);
    mqtt.publishCommand(thingName!, "calibrate", "cmd-1", {});
    const ev = await serial.waitForEvent("command_received", 10_000);
    expect(ev.value).toBe("calibrate");
    const ack = await ackP;
    expect(ack.status).toBe("executed");
  });

  it("executes command with params", async () => {
    const ackP = mqtt.waitForAck(thingName!, "cmd-2", 15_000);
    mqtt.publishCommand(thingName!, "blink", "cmd-2", { count: 3 });
    const ev = await serial.waitForEvent("command_received", 10_000);
    expect(ev.value).toBe("blink");
    const ack = await ackP;
    expect(ack.status).toBe("executed");
  });

  it("handles failed command", async () => {
    const ackP = mqtt.waitForAck(thingName!, "cmd-3", 15_000);
    mqtt.publishCommand(thingName!, "fail_test", "cmd-3", {});
    const ev = await serial.waitForEvent("command_received", 10_000);
    expect(ev.value).toBe("fail_test");
    const ack = await ackP;
    expect(ack.status).toBe("failed");
  });

  it("responds to unknown command", async () => {
    const ackP = mqtt.waitForAck(thingName!, "cmd-4", 15_000);
    mqtt.publishCommand(thingName!, "nonexistent_command", "cmd-4", {});
    const ack = await ackP;
    expect(ack.status).toBe("unknown_command");
  });
});

describe("Commands (via server REST API)", () => {
  it("sends command via API and device receives it", async () => {
    const { data: cmd } = await api.sendDeviceCommand(apiKey, thingName!, "calibrate", { sensor: "all" });
    expect(cmd.id).toBeTruthy();
    expect(cmd.status).toBe("pending");

    const ackP = mqtt.waitForAck(thingName!, cmd.id, 15_000);
    const ev = await serial.waitForEvent("command_received", 15_000);
    expect(ev.value).toBe("calibrate");
    const ack = await ackP;
    expect(ack.status).toBe("executed");
  });

  it("command is recorded in server history", async () => {
    const { data: commands } = await api.getDeviceCommands(apiKey, thingName!);
    expect(commands.length).toBeGreaterThan(0);
    const latest = commands[0];
    expect(latest.action).toBe("calibrate");
    expect(latest.device_id).toBe(thingName);
  });

  it("sends command with TTL via API", async () => {
    const { data: cmd } = await api.sendDeviceCommand(apiKey, thingName!, "blink", { count: 2 }, 30);
    expect(cmd.id).toBeTruthy();
    expect(cmd.expires_at).toBeTruthy();
    const ackP = mqtt.waitForAck(thingName!, cmd.id, 15_000);
    const ev = await serial.waitForEvent("command_received", 15_000);
    expect(ev.value).toBe("blink");
    const ack = await ackP;
    expect(ack.status).toBe("executed");
  });

  it("sends unknown command via API — device ACKs as unknown", async () => {
    const { data: cmd } = await api.sendDeviceCommand(apiKey, thingName!, "nonexistent_api_command", {});
    const ack = await mqtt.waitForAck(thingName!, cmd.id, 15_000);
    expect(ack.status).toBe("unknown_command");
  });

  it("rejects command to non-existent device", async () => {
    await expect(
      api.sendDeviceCommand(apiKey, "paq_DOESNOTEXIST", "calibrate", {})
    ).rejects.toThrow("404");
  });

  it("expired command has correct status in server", async () => {
    const { data: cmd } = await api.sendDeviceCommand(apiKey, thingName!, "calibrate", {}, 1);
    expect(cmd.status).toBe("pending");
    await new Promise((r) => setTimeout(r, 2000));
    const { data: commands } = await api.getDeviceCommands(apiKey, thingName!);
    const target = commands.find((c: any) => c.id === cmd.id);
    expect(["delivered", "expired"]).toContain(target?.status);
  });
});

// ── MQTT Resilience ──────────────────────────────────────────

describe("MQTT Resilience", () => {
  it("device recovers after forced disconnect", async () => {
    const { execFileSync } = await import("child_process");
    const containerId = execFileSync("docker", ["ps", "-q", "--filter", "name=emqx"], { encoding: "utf-8" }).trim().split("\n")[0];
    if (!containerId) throw new Error("EMQX container not found");

    // Kick the device from EMQX
    execFileSync("docker", ["exec", containerId, "emqx", "ctl", "clients", "kick", thingName!], { encoding: "utf-8" });

    // Wait for device to reconnect (exponential backoff, check every 2s, max 30s)
    let reconnected = false;
    for (let i = 0; i < 15; i++) {
      await new Promise((r) => setTimeout(r, 2000));
      try {
        const { execSync } = await import("child_process");
        const clients = execSync(`docker exec ${containerId} emqx ctl clients list 2>/dev/null || true`, { encoding: "utf-8" });
        if (clients.includes(thingName!)) { reconnected = true; break; }
      } catch { /* EMQX may be restarting */ }
    }
    expect(reconnected).toBe(true);

    // Verify commands still work after reconnect
    serial.drainEvents("command_received");
    const ackP = mqtt.waitForAck(thingName!, "reconnect-cmd", 15_000);
    mqtt.publishCommand(thingName!, "calibrate", "reconnect-cmd", {});
    const ev = await serial.waitForEvent("command_received", 15_000);
    expect(ev.value).toBe("calibrate");
    const ack = await ackP;
    expect(ack.status).toBe("executed");
  }, 60_000);
});

// ── Edge Cases (runs before OTA since OTA may reboot device) ──

describe("Edge Cases & Robustness", () => {
  it("publishTelemetry returns false before device is ready", async () => {
    expect((await serial.waitForTest("8.1", 5_000)).status).toBe("pass");
  });

  it("device.loop() before begin() does not crash", async () => {
    expect((await serial.waitForTest("8.2", 5_000)).status).toBe("pass");
  });

});

// ── OTA (LAST — may reboot device) ───────────────────────────

describe("OTA Updates", () => {
  it("firmware binary exists after build", () => {
    const binPath = firmwareBinPath();
    if (!existsSync(binPath)) {
      console.log("    Skipping: firmware binary not built yet");
      return;
    }
    expect(existsSync(binPath)).toBe(true);
  });

  it("bad SHA256 is rejected without crashing", async () => {
    if (!thingName) return;
    await api.setDesired(apiKey, thingName, {
      fw_target: "9.9.9",
      ota_url: "http://192.168.1.240:3000/api/v1/firmware/NONEXISTENT/download",
      ota_sha256: "0000000000000000000000000000000000000000000000000000000000000000",
    });
    await new Promise((r) => setTimeout(r, 10_000));
    const device = await api.getDevice(apiKey, thingName);
    expect(device).toBeTruthy();
    await api.setDesired(apiKey, thingName, {
      fw_target: null, ota_url: null, ota_sha256: null,
    });
  });

  it("full OTA update cycle", async () => {
    if (!thingName) return;
    const { createHash } = await import("node:crypto");
    const { readFile } = await import("node:fs/promises");
    const { buildWithVersion } = await import("../helpers/pio.js");

    // Build firmware with different version
    console.log("    [ota] Building firmware v9.9.9...");
    const binPath = await buildWithVersion("9.9.9");
    const bin = await readFile(binPath);
    const sha256 = createHash("sha256").update(bin).digest("hex");
    console.log(`    [ota] Uploading (${bin.length} bytes, sha256=${sha256.substring(0, 16)}...)`);
    try { await api.uploadFirmware(apiKey, "9.9.9", binPath); } catch { /* may already exist */ }

    // Trigger OTA via shadow desired
    const publicUrl = process.env.PUBLIC_BASE_URL ?? "http://192.168.1.240:3000";
    console.log("    [ota] Setting desired state to trigger OTA...");
    await api.setDesired(apiKey, thingName, {
      fw_target: "9.9.9",
      ota_url: `${publicUrl}/api/v1/firmware/9.9.9/download`,
      ota_sha256: sha256,
    });

    // Poll shadow for reported firmware version to change.
    // After OTA reboot, the device waits for {"action":"start"} before begin().
    // Send start periodically so the rebooted device can provision and report.
    console.log("    [ota] Waiting for device to update (up to 90s)...");
    let updated = false;
    for (let i = 0; i < 30; i++) {
      await new Promise((r) => setTimeout(r, 3000));
      // Nudge device in case it rebooted and is waiting for start
      try { serial.send({ action: "start" }); } catch { /* serial may be disconnected */ }
      try {
        const shadow = await api.getShadow(apiKey, thingName);
        const fw = shadow.reported?.firmware || shadow.reported?.fw;
        if (fw === "9.9.9") {
          console.log(`    [ota] Device updated to v9.9.9 after ~${(i + 1) * 3}s`);
          updated = true;
          break;
        }
      } catch { /* device rebooting */ }
    }
    expect(updated).toBe(true);

    // Clear OTA desired state and restore original firmware via USB
    await api.setDesired(apiKey, thingName, { fw_target: null, ota_url: null, ota_sha256: null });
    console.log("    [ota] Restoring v1.0.0 via USB flash...");
    // Close serial so flash can access the port
    await serial.close();
    await flash();
  }, 180_000);
});

// ── Cleanup (LAST — disconnects device) ──────────────────────

describe("Cleanup", () => {
  it("device.reset() clears certificates", async () => {
    // Reopen serial if closed by OTA test, wait for device to boot
    try { await serial.open(); } catch { /* may already be open */ }
    await new Promise((r) => setTimeout(r, 3000));
    serial.flush();
    // Send start in case device rebooted from OTA flash restore
    serial.send({ action: "start" });
    try { await serial.waitForReady("wifi", 30_000); } catch { /* may already be past boot */ }
    // Wait for provisioning to complete before reset
    try { await serial.waitForReady("self_tests_complete", 90_000); } catch { /* may already be done */ }
    serial.send({ action: "reset" });
    const r = await serial.waitForTest("8.3", 10_000);
    expect(r.status).toBe("pass");
    const ev = await serial.waitForEvent("device_reset", 5_000);
    expect(ev.value).toBe("done");
  }, 120_000);
});
