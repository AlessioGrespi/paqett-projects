const API_URL = process.env.PAQETT_API_URL ?? "http://localhost:3000";

async function request(
  method: string,
  path: string,
  apiKey: string,
  body?: unknown
): Promise<unknown> {
  const headers: Record<string, string> = {
    "X-API-Key": apiKey,
  };
  if (body !== undefined) headers["Content-Type"] = "application/json";

  const res = await fetch(`${API_URL}${path}`, {
    method,
    headers,
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });

  if (!res.ok) {
    const text = await res.text();
    throw new Error(`API ${method} ${path} failed (${res.status}): ${text}`);
  }

  const contentType = res.headers.get("content-type") ?? "";
  if (contentType.includes("application/json")) {
    return res.json();
  }
  return res.text();
}

/** Create a new API key. Returns the raw key string. */
export async function createApiKey(): Promise<string> {
  const res = await fetch(`${API_URL}/api/v1/api-keys`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name: "sdk-test-runner" }),
  });
  if (!res.ok) throw new Error(`Failed to create API key: ${await res.text()}`);
  const data = (await res.json()) as { key: string };
  return data.key;
}

/** Check server health. */
export async function healthCheck(): Promise<boolean> {
  try {
    const res = await fetch(`${API_URL}/api/v1/health`);
    return res.ok;
  } catch {
    return false;
  }
}

/** List all devices. */
export async function getDevices(apiKey: string): Promise<{ devices: Array<{ device_id: string; status: string }> }> {
  return request("GET", "/api/v1/devices", apiKey) as Promise<{ devices: Array<{ device_id: string; status: string }> }>;
}

/** Get a single device. */
export async function getDevice(apiKey: string, deviceId: string): Promise<{ id: string; device_id: string; status: string }> {
  const res = await request("GET", `/api/v1/devices/${deviceId}`, apiKey) as { data: { id: string; status: string } };
  return { ...res.data, device_id: res.data.id };
}

/** Get device shadow. */
export async function getShadow(
  apiKey: string,
  deviceId: string
): Promise<{ reported: Record<string, unknown>; desired: Record<string, unknown>; delta: Record<string, unknown> }> {
  const res = await request("GET", `/api/v1/devices/${deviceId}/shadow`, apiKey) as {
    data: { reported: Record<string, unknown>; desired: Record<string, unknown>; delta: Record<string, unknown> };
  };
  return res.data;
}

/** Patch desired shadow state. Wraps in {state: ...} as expected by API. */
export async function setDesired(
  apiKey: string,
  deviceId: string,
  desired: Record<string, unknown>
): Promise<unknown> {
  return request("PATCH", `/api/v1/devices/${deviceId}/shadow/desired`, apiKey, { state: desired });
}

/** Get telemetry readings. */
export async function getTelemetry(
  apiKey: string,
  deviceId: string
): Promise<{ readings: Array<Record<string, unknown>> }> {
  return request("GET", `/api/v1/telemetry?device_id=${deviceId}`, apiKey) as Promise<{
    readings: Array<Record<string, unknown>>;
  }>;
}

/** Get latest telemetry reading. */
export async function getLatestTelemetry(
  apiKey: string,
  deviceId: string
): Promise<{ readings: Array<Record<string, unknown>> }> {
  const res = await request("GET", `/api/v1/telemetry/latest?device_id=${deviceId}`, apiKey) as { data: Record<string, unknown> };
  // Normalize: API returns single {data: ...}, wrap in array for consistency
  return { readings: res.data ? [res.data] : [] };
}

// ── Device Commands (via REST API) ─────────────────────────────

export interface DeviceCommand {
  id: string;
  device_id: string;
  action: string;
  payload: Record<string, unknown>;
  status: string;
  expires_at: string;
  acked_at: string | null;
  created_at: string;
}

/** Send a command to a device via the REST API. */
export async function sendDeviceCommand(
  apiKey: string,
  deviceId: string,
  action: string,
  payload: Record<string, unknown> = {},
  ttl?: number
): Promise<{ data: DeviceCommand }> {
  const body: Record<string, unknown> = { action, payload };
  if (ttl !== undefined) body.ttl = ttl;
  return request("POST", `/api/v1/devices/${deviceId}/commands`, apiKey, body) as Promise<{ data: DeviceCommand }>;
}

/** List commands sent to a device. */
export async function getDeviceCommands(
  apiKey: string,
  deviceId: string,
  status?: string
): Promise<{ data: DeviceCommand[] }> {
  const qs = status ? `?status=${status}` : "";
  return request("GET", `/api/v1/devices/${deviceId}/commands${qs}`, apiKey) as Promise<{ data: DeviceCommand[] }>;
}

/** Upload firmware binary. */
export async function uploadFirmware(
  apiKey: string,
  version: string,
  binPath: string
): Promise<unknown> {
  const { readFile } = await import("node:fs/promises");
  const bin = await readFile(binPath);
  const form = new FormData();
  form.append("file", new Blob([bin]), "firmware.bin");
  form.append("version", version);

  const res = await fetch(`${API_URL}/api/v1/firmware`, {
    method: "POST",
    headers: { "X-API-Key": apiKey },
    body: form,
  });
  if (!res.ok) throw new Error(`Firmware upload failed: ${await res.text()}`);
  return res.json();
}

/** Trigger OTA rollout. */
export async function createRollout(
  apiKey: string,
  version: string,
  devices: string[]
): Promise<{ id: string }> {
  return request("POST", `/api/v1/firmware/${version}/rollout`, apiKey, { device_ids: devices }) as Promise<{ id: string }>;
}

/** Get rollout status. */
export async function getRollout(apiKey: string, rolloutId: string): Promise<{ status: string; updated_devices: number }> {
  return request("GET", `/api/v1/rollouts/${rolloutId}`, apiKey) as Promise<{ status: string; updated_devices: number }>;
}
