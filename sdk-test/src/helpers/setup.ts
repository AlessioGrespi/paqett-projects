import { DeviceSerial, detectPort } from "./serial.js";
import { TestMqtt } from "./mqtt.js";
import * as api from "./api.js";

/**
 * Shared test context — serial, MQTT, API key, thing name.
 * Initialized once by the first test suite and reused across all suites.
 * Uses globalThis to ensure singleton across ESM module instances in vitest.
 */
export interface TestContext {
  serial: DeviceSerial;
  mqtt: TestMqtt;
  apiKey: string;
  thingName: string | null; // set after provisioning
}

declare global {
  // eslint-disable-next-line no-var
  var __paqettTestCtx: TestContext | undefined;
}

/**
 * Get or create the shared test context.
 * First call: detects port, opens serial, connects MQTT, creates API key.
 */
export async function getTestContext(): Promise<TestContext> {
  if (globalThis.__paqettTestCtx) return globalThis.__paqettTestCtx;

  // Preflight checks
  const healthy = await api.healthCheck();
  if (!healthy) {
    throw new Error(
      "Server not healthy at http://localhost:3000/api/v1/health. Is docker-compose up?"
    );
  }

  const portPath = await detectPort();
  console.log(`  Serial port: ${portPath}`);

  const serial = new DeviceSerial(portPath);
  await serial.open();

  const mqtt = new TestMqtt();
  await mqtt.connect();

  const apiKey = await api.createApiKey();
  console.log(`  API key: ${apiKey.substring(0, 20)}...`);

  globalThis.__paqettTestCtx = { serial, mqtt, apiKey, thingName: null };
  return globalThis.__paqettTestCtx;
}

/** Cleanup — close serial and MQTT connections. */
export async function teardown(): Promise<void> {
  const ctx = globalThis.__paqettTestCtx;
  if (!ctx) return;
  await ctx.serial.close();
  await ctx.mqtt.disconnect();
  globalThis.__paqettTestCtx = undefined;
}
