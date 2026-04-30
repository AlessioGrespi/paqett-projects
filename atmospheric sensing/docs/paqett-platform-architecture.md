# Paqett Platform Architecture

> **"Supabase, but for IoT."**
> Open-source device platform. MQTT + certs + telemetry storage + direct Postgres access.
> Self-host or use our cloud. Ship hardware products without building infrastructure.

---

## Table of Contents

1. [Vision & Positioning](#1-vision--positioning)
2. [Platform Overview](#2-platform-overview)
3. [Identity & Certificates](#3-identity--certificates)
4. [MQTT Broker](#4-mqtt-broker)
5. [Device Provisioning](#5-device-provisioning)
6. [Data Architecture](#6-data-architecture)
7. [Device Shadows / Digital Twins](#7-device-shadows--digital-twins)
8. [Gateways & Multi-Protocol](#8-gateways--multi-protocol)
9. [Data Access Patterns](#9-data-access-patterns)
10. [Rules Engine](#10-rules-engine)
11. [OTA & Fleet Management](#11-ota--fleet-management)
12. [Multi-Tenancy & Organizations](#12-multi-tenancy--organizations)
13. [Device Lifecycle](#13-device-lifecycle)
14. [Observability](#14-observability)
15. [SDK Ecosystem](#15-sdk-ecosystem)
16. [Competitive Landscape](#16-competitive-landscape)
17. [Infrastructure & Hosting](#17-infrastructure--hosting)
18. [Roadmap](#18-roadmap)

---

## 1. Vision & Positioning

### The Problem

IoT infrastructure is stuck between two bad options:

- **Hyperscaler platforms** (AWS IoT Core, Azure IoT Hub): powerful but expensive, complex, vendor-locked
- **Hobbyist platforms** (Arduino Cloud, Blynk, Datacake): nice UI, can't build real products on them

Google Cloud IoT Core shut down in August 2023, leaving thousands of teams scrambling. ThingsBoard is the only serious open-source alternative — but it's a Java monolith with an intentionally crippled community edition.

### The Opportunity

```
                    Developer Experience
                           ^
                           |
            Paqett         |
              *            |         AWS IoT
                           |           *
       Supabase-like       |    Powerful but
       DX, open source     |    overwhelming
                           |
  -------------------------+------------------------->
  Hobbyist / Limited       |         Full Platform
                           |
        Arduino *          |
        Blynk *            |     ThingsBoard *
                           |     Complex, Java,
    Nice UI, can't         |     crippled CE
    build real products    |
                           |
```

### The Pitch

**For hardware developers**: `npm install @paqett/sdk`, drop in your config, and your device is connected. Direct SQL access to your telemetry. Webhooks, MQTT-WS, REST API — pick how you want your data.

**For companies**: Self-host or use our cloud. No vendor lock-in. No per-message pricing surprises. Open-source core with managed hosting.

### The Playbook

The Supabase model: expose Postgres directly, wrap it with auth/storage/realtime. Developers already know MQTT and Postgres. Don't make them learn proprietary APIs.

```
Phase 1: Self-host broker + provisioning API for own devices
Phase 2: Multi-tenant — isolated namespaces, API keys, dashboards
Phase 3: CLI + SDK ecosystem ("paqett deploy", device just works)
Phase 4: Open-source core, offer hosted version
```

---

## 2. Platform Overview

```
+---------------------------------------------------------------+
|                      Paqett Platform                          |
+---------------------------------------------------------------+
|                                                               |
|  +-----------------+  +------------------+  +---------------+ |
|  | Identity & Auth |  | Device Lifecycle |  | Multi-tenant  | |
|  | . MCU serial    |  | . Provisioning   |  | . Orgs        | |
|  | . Cert CA       |  | . Shadows/Twins  |  | . Roles       | |
|  | . API keys      |  | . OTA + rollout  |  | . Limits      | |
|  | . JWT (users)   |  | . Cert rotation  |  | . Billing     | |
|  |                 |  | . Decommission   |  |               | |
|  +-----------------+  +------------------+  +---------------+ |
|                                                               |
|  +-----------------+  +------------------+  +---------------+ |
|  | Connectivity    |  | Data             |  | Access        | |
|  | . MQTT (direct) |  | . TimescaleDB    |  | . MQTT-WS     | |
|  | . Thread/Matter |  | . Shadows (KV)   |  | . Webhooks    | |
|  | . BLE           |  | . Codec registry |  | . REST API    | |
|  | . LoRaWAN (TTN) |  | . Rules engine   |  | . Direct SQL  | |
|  | . LoRaWAN (prv) |  | . Archive (S3)   |  | . Export      | |
|  | . Gateways      |  | . Audit log      |  | . MQTT sub    | |
|  +-----------------+  +------------------+  +---------------+ |
|                                                               |
|  +-----------------+  +------------------+  +---------------+ |
|  | Fleet           |  | Observability    |  | SDKs          | |
|  | . Groups        |  | . Broker metrics |  | . Arduino     | |
|  | . Bulk commands |  | . Ingest lag     |  | . ESP-IDF     | |
|  | . Spatial/geo   |  | . Platform health|  | . Zephyr      | |
|  | . Dynamic query |  | . Per-tenant     |  | . JS/Python   | |
|  |                 |  |                  |  | . CLI         | |
|  +-----------------+  +------------------+  +---------------+ |
+---------------------------------------------------------------+
```

### Full System Diagram

```
                         +--------------------------------------+
  WiFi Devices ---mTLS-->|                                      |
                         |        MQTT Broker (EMQX)            |
  Thread Border ---mTLS->|        . mTLS :8883 (devices)        |
  Router (GW)            |        . WSS  :8084 (dashboards)     |
                         |        . Webhook --> Ingest          |
  BLE Gateway ----mTLS-->|                                      |
                         +------------------+-------------------+
                                            |
  LoRa Sensors --RF--> LoRa GW -->  ChirpStack --MQTT--+
                                   (or webhook)         |
                                            |           |
                         +------------------v-----------v------+
                         |  Ingest Worker                       |
                         |  (protocol-agnostic)                 |
                         |  Normalizes to:                      |
                         |    { serial, timestamp, payload,     |
                         |      source }                        |
                         +------------------+-------------------+
                                            |
              +-----------------------------+-------------------+
              |                             |                   |
     +--------v--------+     +-------------v-----------+  +----v-----+
     |  App DB          |     |  Telemetry DB           |  | Valkey   |
     |  (Postgres)      |     |  (Postgres+TimescaleDB) |  | Cache    |
     |  . users         |     |  . telemetry hypertable |  | . shadow |
     |  . devices       |     |  . continuous aggregates|  | . latest |
     |  . orgs          |     |  . compression policies |  | . rate   |
     |  . api_keys      |     |  . retention policies   |  | . limits |
     |  . alert_rules   |     |                         |  |          |
     |  . firmware_ver  |     |                         |  |          |
     |  . audit_log     |     |                         |  |          |
     +---------+--------+     +-------------+-----------+  +----+-----+
               |                            |                   |
     +---------v----------------------------v-------------------v------+
     |  Object Storage (Cloudflare R2 / MinIO / Backblaze B2)          |
     |  . telemetry archive (parquet, lifecycle --> cold tier)          |
     |  . firmware binaries (OTA, signed URLs, versioned)              |
     |  . bulk exports (CSV, JSON lines, parquet)                      |
     +-----------------------------------------------------------------+
     |  Certificate Authority (step-ca)                                |
     |  . Root CA (offline, air-gapped)                                |
     |  . Intermediate CA (signing server)                             |
     |  . CRL/OCSP (revocation)                                       |
     +-----------------------------------------------------------------+
```

---

## 3. Identity & Certificates

### Device Identity Model

Every device is identified by its MCU serial number — burned into silicon, immutable. This is the root of trust.

```
MCU serial (burned in silicon, immutable)
    |
    v
Platform identity: thing_name derived from serial
    e.g., "paq_AABBCCDD1122"
    |
    v
Certificate CN: same thing_name (for direct-connect devices)
    |
    v
Topic namespace: devices/{thing_name}/*
```

The device is **always** `paq_AABBCCDD1122` regardless of how it connects — WiFi direct, through a Thread border router, via a LoRa gateway, or over BLE. The transport is a routing detail, not an identity relationship.

### Own PKI (Certificate Authority)

Instead of paying AWS IoT Core for cert management, Paqett runs its own PKI:

```
Paqett Root CA (offline, air-gapped)
    |
    +-- Paqett Intermediate CA (on provisioning server)
            |
            +-- Device Certificate (per-device, signed by intermediate)
            +-- Device Certificate (per-device, signed by intermediate)
            +-- ...
```

**Why intermediate CA**: If the intermediate is compromised, revoke it and issue a new one. Devices with certs signed by the old intermediate get rejected. The root stays safe.

**Tool**: `step-ca` (Smallstep) for automated certificate lifecycle, or raw `openssl` for simplicity.

### Certificate Properties

| Property | Value |
|---|---|
| Algorithm | EC P-256 (ECDSA) |
| CN | `paq_{MCU_SERIAL}` |
| Validity | 1 year (auto-renewal via shadow desired state) |
| Key generation | Device-side CSR preferred (private key never leaves device) |
| Revocation | CRL published by intermediate CA, checked by broker |

---

## 4. MQTT Broker

### Why EMQX over Mosquitto

| Feature | Mosquitto | EMQX |
|---|---|---|
| Clustering | No | Native |
| WebSocket | Plugin | Built-in |
| JWT auth | External plugin | Built-in plugin |
| ACL via HTTP callback | External plugin | Built-in |
| Dashboard | No | Built-in |
| Multi-tenancy | Manual | Supported |
| Performance | ~100K connections | ~100M connections |
| License | EPL-2.0 | Apache 2.0 |

Mosquitto is fine for starting (single VPS, simple config). EMQX is the target for production/PaaS.

### Broker Configuration

```
Listeners:
  . :8883  mTLS    (devices)     -- client cert required
  . :8084  WSS     (dashboards)  -- JWT auth
  . :1883  TCP     (internal)    -- localhost only, for ingest workers

ACL (device connections via mTLS):
  pattern readwrite devices/%c/#
  -- %c = client CN from certificate = thing_name
  -- each device can only pub/sub to its own topic tree

ACL (dashboard connections via JWT):
  HTTP callback --> API validates JWT, returns allowed topics
  -- user can only see devices they own

Retained messages:
  . devices/{id}/shadow/delta   -- always retained
  -- enables instant state sync on reconnect
```

### Topic Structure

```
devices/{thing_name}/
    |-- telemetry              <-- device publishes sensor data
    |-- status                 <-- device publishes connection state
    |-- shadow/
    |   |-- reported           <-- device publishes current state
    |   |-- desired            <-- cloud publishes intended state
    |   |-- delta              <-- platform publishes diff (retained)
    |   |-- reported/accepted  <-- platform confirms save
    |   +-- desired/accepted   <-- platform confirms save
    |-- commands               <-- cloud publishes one-shot commands
    +-- commands/ack           <-- device acknowledges command receipt

gateways/{gw_id}/
    |-- status                 <-- gateway health
    +-- commands/{child_id}    <-- commands routed to child devices
```

---

## 5. Device Provisioning

### Flow: Direct WiFi Device

```
+-------------+                    +------------------+          +--------+
|  ESP32-C6   |                    | Provisioning API |          | Broker |
| (first boot)|                    |                  |          |        |
+------+------+                    +--------+---------+          +---+----+
       |                                    |                        |
       | 1. HTTPS POST /api/v1/provision    |                        |
       |    { serial: "AABB...",            |                        |
       |      firmware: "1.0.0" }           |                        |
       |    Header: X-API-Key: {fleet_key}  |                        |
       +----------------------------------->|                        |
       |                                    |                        |
       |                          2. Validate API key                |
       |                          3. Generate cert+key               |
       |                             (sign with intermediate CA)     |
       |                          4. Create device record in DB      |
       |                                    |                        |
       | 5. Response:                       |                        |
       |    { cert_pem,                     |                        |
       |      private_key_pem,              |                        |
       |      ca_chain_pem,                 |                        |
       |      mqtt_endpoint,                |                        |
       |      thing_name }                  |                        |
       |<-----------------------------------+                        |
       |                                                             |
       | 6. Save to NVS:                                             |
       |    device_cert, private_key,                                |
       |    root_ca, thing_name                                      |
       |                                                             |
       | 7. Connect with mTLS                                        |
       +------------------------------------------------------------>|
       |                                                             |
       | 8. Subscribe: devices/{thing_name}/shadow/delta             |
       +------------------------------------------------------------>|
       |                                                             |
       | 9. Publish: devices/{thing_name}/shadow/reported            |
       |    { fw: "1.0.0", status: "online" }                       |
       +------------------------------------------------------------>|
```

### Flow: Subsequent Boots

```
+-------------+                              +--------+
|  ESP32-C6   |                              | Broker |
| (warm boot) |                              |        |
+------+------+                              +---+----+
       |                                         |
       | 1. Load cert+key from NVS               |
       |                                         |
       | 2. Connect with mTLS                    |
       +---------------------------------------->|
       |                                         |
       | 3. Subscribe: devices/{id}/shadow/delta |
       +---------------------------------------->|
       |                                         |
       | 4. Broker delivers retained delta       |
       |    (if any desired state pending)        |
       |<----------------------------------------+
       |                                         |
       | 5. Ready for telemetry                  |
```

### Flow: Gateway Child Device

```
+----------+    +----------+               +-----------+         +--------+
| Thread   |    | Border   |               | Platform  |         | Broker |
| Sensor   |    | Router   |               | API       |         |        |
+----+-----+    +----+-----+               +-----+-----+         +---+----+
     |               |                           |                    |
     | (discovered   |                           |                    |
     |  on Thread    |                           |                    |
     |  mesh)        |                           |                    |
     |               |                           |                    |
     |               | POST /provision/child     |                    |
     |               | { gateway_id: "gw_abc",   |                    |
     |               |   child_serial: "EEFF..", |                    |
     |               |   protocol: "thread" }    |                    |
     |               +-------------------------->|                    |
     |               |                           |                    |
     |               |         { child_thing_name: "paq_EEFF.." }    |
     |               |<--------------------------+                    |
     |               |                           |                    |
     | (sensor data) |                           |                    |
     +-------------->|                           |                    |
     |               |                           |                    |
     |               | Publish: devices/paq_EEFF../telemetry         |
     |               | (gateway publishes on behalf of child)        |
     |               +--------------------------------------------->|
```

**No cert for child devices** — the gateway vouches for them. The gateway holds the mTLS cert. Child devices are registered via the gateway's API call.

### CSR vs Server-Generated Keys

| Approach | Security | Complexity | Use when |
|---|---|---|---|
| **Device generates CSR** | Private key never leaves device | Device needs crypto capability | Production, security-critical |
| **Server generates both** | Key transmitted over HTTPS | Simpler device code | Prototyping, constrained devices |

ESP32-C6 can generate EC P-256 keys in ~200ms with hardware acceleration — CSR approach is viable and preferred.

---

## 6. Data Architecture

### Dual Database Strategy

```
+----------------------------+     +----------------------------+
|  App DB (Postgres)         |     |  Telemetry DB              |
|                            |     |  (Postgres + TimescaleDB)  |
+----------------------------+     +----------------------------+
|  users                     |     |  telemetry (hypertable)    |
|  devices                   |     |    . auto-partitioned by   |
|  organizations             |     |      time (7-day chunks)   |
|  api_keys                  |     |    . compressed after 7d   |
|  alert_rules               |     |    . retention policy      |
|  alert_history             |     |      (configurable/org)    |
|  firmware_versions         |     |                            |
|  device_profiles           |     |  continuous aggregates     |
|  device_shadows            |     |    . hourly_rollups        |
|  audit_log                 |     |    . daily_rollups         |
|  billing_usage             |     |                            |
|  webhook_configs           |     |  Indexes:                  |
|  rule_definitions          |     |    . (device_id, time)     |
|                            |     |    . (org_id, time)        |
+----------------------------+     +----------------------------+
         |                                     |
         |  device_id is a plain string        |
         |  (thing_name) -- NO cross-db FK     |
         |  Telemetry ingest has zero          |
         |  dependency on app DB               |
         +-------------------------------------+
```

**Why separate databases:**
- Different access patterns (OLTP vs time-series append)
- Different scaling profiles (app DB grows slowly, telemetry grows fast)
- Telemetry ingest continues even if app DB is down for migration
- TimescaleDB hypertables have specific configuration needs (chunk intervals, compression, retention)

### TimescaleDB Configuration

```sql
-- Create the hypertable
CREATE TABLE telemetry (
    time        TIMESTAMPTZ NOT NULL,
    device_id   TEXT NOT NULL,
    org_id      TEXT NOT NULL,
    payload     JSONB NOT NULL,
    source      TEXT,  -- "wifi", "thread", "ble", "lorawan_ttn", "lorawan_cs"
    UNIQUE (device_id, time)
);

SELECT create_hypertable('telemetry', by_range('time'));

-- Continuous aggregates (materialized rollups)
CREATE MATERIALIZED VIEW hourly_rollups
WITH (timescaledb.continuous) AS
SELECT
    time_bucket('1 hour', time) AS bucket,
    device_id,
    org_id,
    avg((payload->>'temperature')::float) AS avg_temp,
    min((payload->>'temperature')::float) AS min_temp,
    max((payload->>'temperature')::float) AS max_temp,
    avg((payload->>'humidity')::float) AS avg_humidity,
    count(*) AS sample_count
FROM telemetry
GROUP BY bucket, device_id, org_id;

-- Compression after 7 days
ALTER TABLE telemetry SET (
    timescaledb.compress,
    timescaledb.compress_segmentby = 'device_id'
);
SELECT add_compression_policy('telemetry', INTERVAL '7 days');

-- Retention (configurable per org, default 90 days)
SELECT add_retention_policy('telemetry', INTERVAL '90 days');
```

### Deduplication

For devices reachable by multiple gateways/networks:

```sql
INSERT INTO telemetry (time, device_id, org_id, payload, source)
VALUES ($1, $2, $3, $4, $5)
ON CONFLICT (device_id, time) DO NOTHING;
```

TimescaleDB handles out-of-order inserts natively — a gateway that reconnects and dumps hours of buffered readings just works.

### Cache Layer (Valkey)

```
Key patterns:
  shadow:{device_serial}          --> JSON shadow state
  device:{device_id}:latest       --> last telemetry reading
  device:{device_id}:status       --> online/offline + last_seen
  rate_limit:{org_id}             --> request counter (sliding window)
  rule_kv:{org_id}:{key}          --> per-org KV for rule state
  session:{token}                 --> dashboard user session

Write-through:
  Ingest worker writes to TimescaleDB + updates Valkey latest/status
  Shadow service writes to Postgres + invalidates Valkey shadow

Read path:
  Dashboard hits Valkey first --> fallback to DB on miss
```

### Object Storage (Cloudflare R2 / MinIO / Backblaze B2)

| Bucket | Purpose | Access pattern |
|---|---|---|
| `telemetry-archive` | Parquet-compressed historical data | Write: scheduled archiver. Read: rare, bulk export |
| `firmware` | OTA firmware binaries | Write: developer upload. Read: signed URL to device |
| `exports` | User-requested data exports | Write: async export job. Read: download link |

---

## 7. Device Shadows / Digital Twins

### Concept

A shadow is the platform's last-known state of a device + the desired state the cloud wants. It works even when the device is offline.

```
+--------------------------------------+
|            Shadow                     |
+------------------+-------------------+
|    REPORTED       |    DESIRED        |
|  (device says)    |  (cloud wants)    |
+------------------+-------------------+
|  valve: "open"    |  valve: "closed"  |
|  fw: "1.3.0"      |  fw: "1.4.0"     |
|  temp: 22.1       |                   |
|  mode: "auto"     |  mode: "manual"   |
+------------------+-------------------+
|              DELTA                    |
|  (computed: desired - reported)       |
|  valve: "closed"                      |
|  fw: "1.4.0"                         |
|  mode: "manual"                       |
|                                       |
|  temp not in delta -- nobody          |
|  "desired" a temperature, it's       |
|  just a reported measurement          |
+---------------------------------------+
```

### Shadow Lifecycle

```
t=0  Device online, synced.
     reported: { valve: "open", mode: "auto" }
     desired:  { valve: "open", mode: "auto" }
     delta:    {}   <-- in sync
     
t=1  User sends command: "close the valve"
     desired:  { valve: "closed" }
     delta:    { valve: "closed" }
     
t=2  IF device is online:
       Platform publishes delta to devices/{id}/shadow/delta
       Device receives, closes valve
       Device publishes reported: { valve: "closed" }
       delta: {}   <-- converged
       
     IF device is OFFLINE:
       Delta sits as retained MQTT message, waiting
       ...hours pass...
       Device reconnects
       Subscribes to devices/{id}/shadow/delta
       Broker delivers retained delta immediately
       Device closes valve
       Device publishes reported: { valve: "closed" }
       delta: {}   <-- converged, eventually
```

### Per-Field Ownership (Conflict Resolution)

Unlike AWS IoT Shadows or Azure Device Twins which treat the entire shadow as a flat conflict domain, Paqett uses per-field ownership:

```yaml
shadow_schema:
  # Device-owned: device decides, cloud can read but not override
  temperature:    ownership: device
  humidity:       ownership: device
  battery:        ownership: device
  mode:           ownership: device     # device can self-manage

  # Cloud-owned: cloud decides, device must comply
  telemetry_interval:  ownership: cloud
  valve_position:      ownership: cloud
  firmware_target:     ownership: cloud
  calibration:         ownership: cloud
```

This eliminates the most common conflict: device changes `mode` to "eco" while offline, cloud had `desired.mode = "auto"` — with per-field ownership, the device owns `mode`, so no conflict.

### Versioning

```
Shadow version: 47
  reported: { valve: "open" }
  desired:  { valve: "open" }

API call: set desired.valve = "closed", expected_version = 47
  --> Success. Version becomes 48.

Concurrent API call: set desired.mode = "manual", expected_version = 47
  --> Rejected. Version is 48. Caller must re-read and retry.
```

Standard optimistic concurrency — prevents race conditions when multiple users/automations modify the same shadow.

### Storage Strategy

```
+-------------------+     +------------------+     +------------------+
|  Valkey (cache)   |     | App Postgres     |     | TimescaleDB      |
|  . Microsecond    |     | (source of truth)|     | (history)        |
|    reads          |     | . Durable        |     | . Shadow change  |
|  . Dashboard hot  |     | . Queryable:     |     |   events as      |
|    path           |     |   "all devices   |     |   time-series    |
|                   |     |    where battery  |     | . "When did      |
|                   |     |    < 20%"        |     |    valve change?" |
+-------------------+     +------------------+     +------------------+

Write: App Postgres (truth) + invalidate Valkey
Read:  Valkey (cache) --> fallback Postgres
Query: Postgres ("all unconverged devices")
History: TimescaleDB (shadow change events)
```

### Device Code

```cpp
// On connect, subscribe to delta
client.subscribe("devices/" + thingName + "/shadow/delta");

// When delta arrives
void onDelta(JsonDocument& delta) {
    if (delta.containsKey("interval")) {
        telemetryInterval = delta["interval"];
    }
    if (delta.containsKey("valve")) {
        setValve(delta["valve"] == "closed");
    }

    // Report back what actually happened
    JsonDocument reported;
    reported["interval"] = telemetryInterval;
    reported["valve"] = getValveState();
    reported["battery"] = getBatteryPercent();

    client.publish("devices/" + thingName + "/shadow/reported",
                   reported.serialize());
}

// Periodic heartbeat
void reportShadow() {
    JsonDocument reported;
    reported["temp"] = getTemperature();
    reported["battery"] = getBatteryPercent();
    reported["valve"] = getValveState();
    reported["fw"] = FIRMWARE_VERSION;

    client.publish("devices/" + thingName + "/shadow/reported",
                   reported.serialize());
}
```

---

## 8. Gateways & Multi-Protocol

### Supported Protocols

| Protocol | Range | Power | Topology | Gateway type |
|---|---|---|---|---|
| **WiFi** | ~30-50m | High | Direct IP | None (device connects directly) |
| **Thread/Matter** | ~30m mesh | Very low | Mesh | Thread Border Router (ESP32-C6!) |
| **BLE** | ~10-30m | Very low | Point-to-point/mesh | Phone or dedicated BLE gateway |
| **LoRaWAN** | 2-15km | Extremely low | Star | LoRa gateway + network server |
| **Zigbee** | ~10-30m mesh | Very low | Mesh with coordinator | Zigbee coordinator |

### Gateway Architecture

```
+------------------------------------------------------------+
|  Gateway (RPi, ESP32-C6 as border router, etc.)            |
+------------------------------------------------------------+
|                                                            |
|  +-----------+  +-----------+  +-----------+               |
|  | Thread    |  | BLE       |  | LoRa      |   Radios     |
|  | Radio     |  | Radio     |  | Radio     |              |
|  +-----+-----+  +-----+-----+  +-----+-----+              |
|        |              |              |                      |
|  +-----v--------------v--------------v-----+               |
|  |        Protocol Handlers                |               |
|  |  . Discover/pair devices                |               |
|  |  . Receive sensor data                  |               |
|  |  . Forward commands to devices          |               |
|  +---------------------+------------------+               |
|                        |                                   |
|  +---------------------v------------------+               |
|  |        Gateway Agent                    |               |
|  |  . Holds the mTLS cert (one per gw)    |               |
|  |  . Multiplexes child devices            |               |
|  |  . Local buffering if offline           |               |
|  |  . Publishes on behalf of children      |               |
|  |  . Runs edge rules locally              |               |
|  +---------------------+------------------+               |
|                        |                                   |
+------------------------|-----------  ----------------------+
                         |
                    mTLS :8883
                         |
                         v
                    MQTT Broker
```

### Device Identity is Transport-Agnostic

The gateway is a **route**, not an **owner**:

```
Traditional (locked):           Paqett (roaming):

Device --belongs_to--> GW      Device --identified_by--> Serial
                                Device --currently_via--> GW (or direct)
                                Route changes, identity doesn't
```

```sql
-- Devices table (permanent identity)
devices:
    serial:           "AABBCCDD1122"         -- immutable, from silicon
    thing_name:       "paq_AABBCCDD1122"     -- derived, permanent
    org_id:           "org_xyz"
    protocol_caps:    ["wifi", "thread"]     -- what this device can speak

-- Routes table (ephemeral, current connection state)
device_routes:
    device_serial:    "AABBCCDD1122"
    connected_via:    "gw_north_wing"        -- or NULL if direct WiFi
    protocol:         "thread"
    last_seen:        2026-04-25T14:30:00Z
    rssi:             -67
```

This is the cellular network model: your phone has an IMSI (identity), connects to whatever tower has the best signal. The network tracks which tower you're on now, but your phone number doesn't change.

### Thread / Matter

```
Thread devices --802.15.4 mesh--> Thread Border Router --WiFi/Eth--> Broker
                                  (can be an ESP32-C6!)
```

- The ESP32-C6 already has a Thread radio (802.15.4)
- Same chip can be a WiFi endpoint, Thread endpoint, or Thread Border Router
- Matter defines standard data models (temperature, humidity, etc.)
- Border Router holds the MQTT cert, child devices registered via API

### LoRaWAN (Dual Network: TTN + Private)

```
+-----------+     +------------+     +-----------------+     +--------+
| LoRa      |     | Public     |     | TTN             |     |        |
| Sensor    |---->| GW (any)   |---->| (public network)|---->|        |
| DevEUI:   |     +------------+     +-----------------+     |        |
| AABB...   |                                                | Paqett |
|           |     +------------+     +-----------------+     | Ingest |
|           |---->| Private    |---->| ChirpStack      |---->|        |
|           |     | GW (yours) |     | (your instance) |     |        |
+-----------+     +------------+     +-----------------+     +--------+
```

Same device registered on both networks. Whichever network delivers the payload first wins. Deduplication by `(device_serial, timestamp)`.

**Why both:** TTN gives free public coverage (urban). Private ChirpStack gives guaranteed coverage (factory, farm, building). Device doesn't know or care which network picks it up.

### Unified Ingest

```
+---------------+
| WiFi Device   |--mTLS----------------------------------+
+---------------+                                        |
+---------------+                                        v
| Thread Device |--via Border Router--mTLS--->  +-----------------+
+---------------+                               |                 |
+---------------+                               |   Ingest        |
| BLE Device    |--via BLE GW--mTLS---------->  |   Worker        |
+---------------+                               |                 |
+---------------+                               |  Normalizes:    |
| LoRa Device   |--RF--> TTN --webhook------->  |  { serial,      |
|               |                               |    timestamp,   |
|               |--RF--> ChirpStack --MQTT--->  |    payload,     |
+---------------+                               |    source }     |
                                                +---------+-------+
                                                          |
                                                          v
                                                   TimescaleDB
                                               (doesn't care how
                                                the data arrived)
```

**The `source` field** is metadata for debugging ("what % of readings come through TTN vs private?") but doesn't affect storage or querying.

### Command Routing to Child Devices

```
User sends:    POST /devices/{child_id}/commands  { "action": "calibrate" }
                    |
Platform:      Looks up child --> belongs to gateway gw_abc
               Publishes to: gateways/{gw_abc}/commands/{child_id}
                    |
Gateway:       Receives, translates to Thread/BLE/LoRa command, delivers
```

---

## 9. Data Access Patterns

Users should be able to access their data however they want. The platform provides a spectrum from real-time to batch:

```
Real-time                                                    Batch
<-------------------------------------------------------------->
MQTT-WS    SSE/WS    Webhooks    API Poll   DB Direct   Export
(live)     (stream)  (push)      (request)  (query)     (file)
```

### 1. MQTT over WebSocket (Live Dashboard)

Browser is literally an MQTT client:

```js
const client = mqtt.connect('wss://mqtt.paqett.io:8084', {
    username: 'dashboard_user_abc',
    password: 'jwt_token_here'
});
client.subscribe('devices/+/telemetry');
client.on('message', (topic, payload) => {
    // update chart in real-time
});
```

Retained messages on shadow/delta topics mean the dashboard shows current state instantly on load — no "waiting for next reading" spinner.

### 2. SSE / WebSocket via API (Enriched Stream)

API holds connection open, pushes enriched events (device name, unit labels, alert context). More structured than raw MQTT.

### 3. Webhooks (Push to User's Infra)

```
User configures:
    URL:     https://their-app.com/api/paqett-webhook
    Events:  telemetry, alerts, device.online, device.offline
    Filter:  device_id = abc123
    Batch:   every 60s OR immediately on alert
```

**Batched, not per-message**: Accumulate readings over configurable window (30s-5min), send one payload with array of readings. Alerts send immediately. Prevents hammering user's endpoint.

### 4. REST API (Historical Queries)

```
GET /api/v1/telemetry
    ?device_id=abc123
    &from=2026-04-20
    &to=2026-04-25
    &resolution=1h        <-- hits continuous aggregates
```

`resolution` param: `raw`, `1min`, `1h`, `1d` — hits pre-computed TimescaleDB rollups.

### 5. Direct SQL Access (The Differentiator)

Give users a **read-only Postgres connection string** to their telemetry:

```
postgresql://user_abc:token@telemetry.paqett.io:5432/user_abc
```

They connect with any Postgres client — psql, DBeaver, Grafana, Metabase, Python pandas, their own ORM. Full SQL power:

```sql
SELECT time_bucket('1 hour', time) AS hour,
       avg((payload->>'temperature')::float),
       max((payload->>'humidity')::float)
FROM telemetry
WHERE device_id = 'paq_abc123'
  AND time > now() - interval '7 days'
GROUP BY hour ORDER BY hour;
```

**Isolation**: Per-tenant schemas with RLS. `CREATE SCHEMA user_abc; CREATE VIEW user_abc.telemetry AS SELECT * FROM telemetry WHERE org_id = 'abc'`. Their connection resolves to their schema. They see only their data.

**This is the key differentiator.** No other IoT platform offers direct SQL access. AWS has proprietary query APIs. Azure has Time Series Insights (deprecated). ThingsBoard has REST. Paqett gives you Postgres.

### 6. Bulk Export

```
POST /api/v1/export
{ "device_id": "abc123", "from": "2026-04-01", "format": "parquet" }

--> { "download_url": "https://storage.paqett.io/exports/..." }
```

Formats: CSV, JSON lines, Parquet. Async job, writes to object storage, notifies via webhook/email.

### 7. MQTT Direct (User's Own Client)

User subscribes to their device topics from their own MQTT client:

```python
import paho.mqtt.client as mqtt
client = mqtt.Client()
client.username_pw_set("user_abc", "jwt_token")
client.tls_set()
client.connect("mqtt.paqett.io", 8883)
client.subscribe("devices/+/telemetry")
```

### Who Wants What

| User type | Primary | Secondary |
|---|---|---|
| Hobbyist building a dashboard | MQTT-WS (live) | API (history) |
| Developer integrating into app | Webhooks + API | Direct DB |
| Data scientist doing analysis | Direct DB (SQL) | Bulk export (Parquet) |
| Enterprise piping into their stack | MQTT direct + Webhooks | Bulk export |
| No-code user on Paqett dashboard | (Handled internally) | -- |

---

## 10. Rules Engine

### Concept

User-deployed functions that run in response to device events. Same model as Supabase Edge Functions / Cloudflare Workers.

```
+---------------------------+    +------------------------+
|  Event Sources            |    |  User Function         |
+---------------------------+    +------------------------+
|  . telemetry received     |    |                        |
|  . shadow changed         |--->|  User's code runs      |
|  . device online/offline  |    |  in V8 isolate         |
|  . device provisioned     |    |  (sandboxed)           |
|  . alert condition met    |    |                        |
|  . OTA completed/failed   |    |                        |
|  . scheduled (cron)       |    |                        |
+---------------------------+    +------------------------+
```

### Rule Definition

```js
// rules/freezing-alert.js
export const trigger = {
    event: "telemetry",
    filter: {
        device_group: "outdoor-stations",
        where: "payload.temperature < 0"
    }
};

export default async function (event, ctx) {
    await ctx.webhook("https://hooks.slack.com/...", {
        text: `${event.device_name} at ${event.payload.temperature} C`
    });

    await ctx.shadow.setDesired(event.device_id, {
        heater: "on"
    });
}
```

```js
// rules/daily-report.js
export const trigger = {
    schedule: "0 8 * * *"  // 8am daily
};

export default async function (event, ctx) {
    const stats = await ctx.sql(`
        SELECT device_id, avg(temperature), min(battery)
        FROM telemetry
        WHERE time > now() - interval '24 hours'
        GROUP BY device_id
        HAVING min(battery) < 30
    `);

    await ctx.webhook(ctx.env.REPORT_WEBHOOK, { devices: stats });
}
```

### Available Primitives

```
ctx.sql(query)                    -- query TimescaleDB (read-only by default)
ctx.shadow.get(device_id)         -- read current shadow
ctx.shadow.setDesired(id, patch)  -- update desired state
ctx.command(device_id, payload)   -- send one-shot command
ctx.webhook(url, body)            -- call external HTTP endpoint
ctx.publish(topic, payload)       -- publish to MQTT topic
ctx.log(message)                  -- structured logging
ctx.env.SOME_SECRET               -- user-configured secrets
ctx.kv.get/set(key, value)        -- per-org KV store (for rule state)
```

**`ctx.sql` is the differentiator**: No other IoT rules engine lets you query raw SQL against telemetry. AWS IoT Rules has a limited SQL-like syntax over single messages. Paqett rules can do cross-device aggregation, historical lookups, joins.

**`ctx.kv` for stateful rules**: "Alert me when temp < 0, but only once per hour." `ctx.kv.set('alerted_' + id, true, { ttl: 3600 })`. Backed by Valkey.

### Execution Model

```
Incoming event
     |
     v
+----------------------------+
|  Router                    |
|  Match event against all   |
|  active rules' triggers    |
|  (fast, no user code)      |
+-------------+--------------+
              |  matched rules
              v
+----------------------------+
|  Isolate Pool              |
|  . V8 isolate per org      |
|  . Memory limit: 128MB     |
|  . CPU time: 50ms (free)   |
|             500ms (pro)    |
|  . Cold start: ~5ms        |
|  . Fully sandboxed         |
|  . ctx.sql scoped to org   |
+----------------------------+
```

### Edge Rules (Gateway-Side)

Run ON the gateway for latency-critical or offline scenarios:

```js
// Deployed to gateway, runs locally
export const trigger = { event: "telemetry", local: true };

export default function (event, ctx) {
    // Sub-millisecond, no cloud round-trip
    if (event.payload.temperature > 80) {
        ctx.localCommand("sprinkler_001", { activate: true });
    }

    // Suppress unchanged values to save bandwidth
    if (event.payload.temperature === ctx.lastValue?.temperature) {
        return { forward: false };
    }
}
```

### Billing

| Tier | Invocations | CPU time | Rules |
|---|---|---|---|
| Free | 100K/mo | 50ms/invocation | 2 |
| Pro | 2M/mo | 500ms/invocation | Unlimited |
| Scale | Usage-based | 1s/invocation | Unlimited + cron |

---

## 11. OTA & Fleet Management

### Device Groups

```
Static groups:   manually assigned
    "building-a-floor-3":     [dev_001, dev_002, dev_003]
    "outdoor-stations":       [dev_050, dev_051]
    "firmware-canary-ring":   [dev_001, dev_050]

Dynamic groups:  query-based, auto-updating
    "low-battery":            WHERE shadow.reported.battery < 20
    "needs-update":           WHERE shadow.reported.fw < "1.4.0"
    "offline-24h":            WHERE last_seen < now() - interval '24h'
```

### OTA Rollout Strategy

```yaml
ota_rollout:
    firmware_version: "1.4.0"
    target_group:     "outdoor-stations"
    strategy:         "canary"
    stages:
      - ring: "canary"    (5%)     -- deploy, wait 24h, check health
      - ring: "early"     (25%)    -- if canary healthy, expand
      - ring: "general"   (100%)   -- full rollout
    rollback_on:
      - crash_rate > 5%
      - telemetry_gap > 30min      -- silence = bad sign
    status: "stage_1_monitoring"
```

**Health signal**: The most reliable indicator of a bad OTA is silence — device stops reporting. Platform monitors telemetry gaps: if canary devices go quiet after update, auto-rollback.

### OTA Flow

```
1. Developer uploads firmware:
   paqett firmware upload --version 1.4.0 --file fw.bin

2. Binary stored in object storage, metadata in app DB

3. Platform sets shadow desired state for target devices:
   desired: { fw: "1.4.0", ota_url: "signed-url" }

4. Device sees delta, downloads firmware, validates SHA256, flashes

5. Device reboots, reports:
   reported: { fw: "1.4.0" }

6. Platform confirms convergence, moves to next rollout stage
```

---

## 12. Multi-Tenancy & Organizations

```sql
organizations:
    id:           "org_xyz"
    name:         "Acme Farms"
    tier:         "pro"
    limits:
        max_devices:      500
        max_msg_per_sec:  1000
        retention_days:   90

members:
    user_id:      "user_123"
    org_id:       "org_xyz"
    role:         "admin"      -- admin | developer | viewer

api_keys:
    org_id:       "org_xyz"
    key_hash:     "sha256:..."
    scopes:       ["devices:read", "telemetry:read", "commands:write"]
    rate_limit:   100/min
```

### Isolation

| Layer | Isolation method |
|---|---|
| MQTT topics | ACL: `{org_id}/devices/{id}/#` or per-cert CN scoping |
| Telemetry DB | RLS or per-org schema |
| API | JWT with org_id claim, validated on every request |
| Rules engine | V8 isolate per org, `ctx.sql` scoped to org's devices |
| Object storage | Prefix: `{org_id}/firmware/`, `{org_id}/exports/` |
| Rate limiting | Per-org counters in Valkey |

### Billing Metering

Count per org per month:
- Messages ingested
- Devices active (distinct device_ids that reported)
- Storage used (TimescaleDB + object storage)
- Rule invocations + CPU time
- API requests

---

## 13. Device Lifecycle

```
manufactured --> registered --> provisioned --> active
                                                  |
                                              suspended
                                             (billing, abuse,
                                              maintenance)
                                                  |
                                           decommissioned
                                          (cert revoked,
                                           data retained
                                           per policy)
```

### Certificate Rotation

Certs expire (1-year validity). Platform tracks expiry dates and pushes renewal via shadow:

```
90 days before expiry:
    shadow desired: { cert_renewal: "required", renew_url: "..." }

Device receives delta, hits renewal endpoint, gets new cert, saves to NVS.
```

### Device Replacement

New hardware replacing an existing logical device:

```
Option A: New identity (new serial = new device)
    Historical data stays on old device record
    User manually "links" old + new in dashboard

Option B: Transfer identity
    POST /devices/{old_serial}/transfer  { new_serial: "..." }
    New device inherits thing_name, historical data, group memberships
    Old device record marked as "replaced"
```

---

## 14. Observability

### Platform Metrics

| Metric | Source | Alert threshold |
|---|---|---|
| Connected clients | EMQX stats | Capacity planning |
| Messages/sec | EMQX stats + ingest counter | > 80% of licensed capacity |
| Ingest lag | Timestamp diff: MQTT publish vs DB write | > 5s |
| Provisioning success rate | API metrics | < 95% |
| Certificate expiry | Cert DB scan | Any device < 30 days |
| Broker memory | EMQX stats | > 80% |
| TimescaleDB chunk size | DB stats | Disk > 80% |
| Per-tenant message rate | Valkey counters | Approaching tier limit |
| Gateway health | Gateway status topics | Gateway offline > 5min |

### Audit Log

```sql
audit_log:
    timestamp:    2026-04-25T14:30:00Z
    org_id:       "org_xyz"
    actor:        "user_123"       -- or "system" or "device_abc"
    action:       "device.command.sent"
    target:       "paq_AABBCCDD1122"
    detail:       { command: "calibrate", source: "api" }
    ip:           "203.0.113.1"
```

---

## 15. SDK Ecosystem

### Device SDKs

```
@paqett/sdk-arduino          -- ESP32, ESP8266 (what exists today)
@paqett/sdk-espidf           -- production ESP-IDF (no Arduino overhead)
@paqett/sdk-zephyr           -- nRF, STM32 (Thread/BLE native)
@paqett/sdk-micropython      -- RPi Pico, hobby market
@paqett/sdk-linux            -- gateway devices (RPi, etc.)
```

### Client SDKs

```
@paqett/js                   -- browser/Node, MQTT-WS + REST
@paqett/python               -- data science, backend integration
```

### CLI

```bash
paqett devices list
paqett devices info paq_AABBCCDD1122
paqett provision --serial AABBCCDD1122
paqett telemetry query --device abc --last 1h
paqett firmware upload --version 1.4.0 --file fw.bin
paqett firmware rollout --version 1.4.0 --group outdoor --strategy canary
paqett rules deploy ./rules/
paqett rules dev           # local emulator
paqett logs tail --device abc
paqett shadow get paq_abc123
paqett shadow set paq_abc123 --desired '{"valve":"closed"}'
```

### Device SDK Config

```cpp
struct PaqettConfig {
    WiFi: { ssid, password }
    API:  { endpoint, key }           // for provisioning
    MQTT: {
        endpoint,                     // your broker
        port,                         // 8883 for TLS
        clientId                      // uses thing_name after provisioning
    }
    Features: {
        enable_telemetry,
        enable_ota,
        enable_shadow,
        telemetry_interval_ms
    }
};
```

---

## 16. Competitive Landscape

### Managed Platforms

| Platform | Strength | Weakness | Paqett advantage |
|---|---|---|---|
| AWS IoT Core | Full feature set | $$$, complexity, vendor lock | Self-hosted, simple DX, direct SQL |
| Azure IoT Hub | Enterprise features | Complex, Azure ecosystem | Open source, transparent pricing |
| HiveMQ Cloud | Great MQTT broker | Broker only, no storage | Full platform |
| Particle | Hardware + cloud | Locked to their hardware | Any hardware |
| Arduino Cloud | Easy to start | Can't scale, toy-grade | Production-ready |
| Blynk | Nice mobile dashboard | Weak data access | Direct SQL, webhooks, full API |
| Ubidots | Decent API | Proprietary query layer | Direct SQL |

### Open Source

| Platform | Strength | Weakness | Paqett advantage |
|---|---|---|---|
| ThingsBoard | Full platform, closest comp | Java monolith, crippled CE | Modern stack, real open source, DX |
| Mainflux/SuperMQ | Good architecture | Small community, sparse docs | SDK ecosystem, direct SQL |
| EMQX | Excellent broker | Broker only | Full platform built on EMQX |
| ChirpStack | Best LoRaWAN stack | LoRa only | Multi-protocol, ChirpStack as integration |

### What Nobody Does

1. **Direct SQL access to telemetry** — every platform has proprietary query APIs
2. **Developer-first DX** — simple SDK config, `paqett deploy`, familiar tools
3. **Transparent pricing at scale** — no per-message surprises
4. **Real open source with hosted option** — not crippled CE to upsell
5. **Multi-protocol with unified data** — same device, any transport, one data model

---

## 17. Infrastructure & Hosting

### Starting Point (Single VPS)

A single Hetzner VPS (~EUR20/mo) runs everything for hundreds of devices:

```
Single VPS (4 CPU, 16GB RAM):
    . EMQX (or Mosquitto)
    . PostgreSQL + TimescaleDB
    . Valkey
    . API server
    . Ingest worker
    . step-ca
    . Nginx (TLS termination for API/WS)
```

### Scaling Seams

When ready to split:

```
Stage 1: Single VPS (hundreds of devices)
    Everything on one box

Stage 2: Separate data (thousands of devices)
    Broker:     own VPS
    Databases:  managed (Neon for Postgres, Timescale Cloud for telemetry)
    Object:     Cloudflare R2 (already external)
    App:        own VPS

Stage 3: Cluster (tens of thousands)
    Broker:     EMQX cluster (3 nodes)
    Databases:  managed, scaled
    App:        multiple instances behind LB
    Ingest:     dedicated workers

Stage 4: Multi-region (global PaaS)
    Broker:     EMQX cluster per region
    Databases:  read replicas per region
    App:        edge deployed
```

### Cost Comparison

| Scale | AWS IoT Core | Paqett (self-hosted) |
|---|---|---|
| 10 devices, 1 msg/10s | ~$5/mo | ~$5/mo (shared VPS) |
| 100 devices, 1 msg/10s | ~$20-40/mo | ~$10-20/mo (single VPS) |
| 1000 devices, 1 msg/10s | ~$200-400/mo | ~$50-100/mo (split infra) |
| 10K devices | ~$2000-4000/mo | ~$200-500/mo |

---

## 18. Roadmap

### Phase 1: Foundation (Now)
- [ ] Self-hosted MQTT broker (EMQX) with mTLS
- [ ] Certificate Authority (step-ca) + provisioning API
- [ ] Device SDK (Arduino/ESP32) — adapt existing paqett-device-sdk
- [ ] Basic telemetry ingest to TimescaleDB
- [ ] REST API for device management + telemetry queries

### Phase 2: Core Platform
- [ ] Device shadows (reported/desired/delta)
- [ ] Webhook delivery
- [ ] MQTT WebSocket for live dashboards
- [ ] OTA firmware delivery
- [ ] Basic dashboard UI

### Phase 3: Multi-Protocol
- [ ] Gateway registration + child device provisioning
- [ ] Thread/Matter support (ESP32-C6 as border router)
- [ ] BLE gateway support
- [ ] LoRaWAN integration (ChirpStack + TTN)

### Phase 4: PaaS Features
- [ ] Multi-tenancy (orgs, roles, isolation)
- [ ] Rules engine (V8 isolates)
- [ ] Direct SQL access for users
- [ ] CLI tool
- [ ] Client SDKs (JS, Python)

### Phase 5: Scale & Open Source
- [ ] EMQX clustering
- [ ] Canary OTA rollouts
- [ ] Fleet management (groups, bulk operations)
- [ ] Open-source core release
- [ ] Hosted cloud offering

---

## Appendix: Data Schema Reference

### Codec / Device Profile Registry

Different devices send different payloads. A codec normalizes them:

```js
device_profiles:
    id:            "profile_atmo_v1"
    name:          "Atmospheric Station v1"
    codec:         "javascript"    // or "cayenneLPP", "protobuf", "cbor"
    decode_fn:     "function decode(bytes) { return { temp: bytes[0]/2 } }"
    schema:
        temperature:  { unit: "C", min: -40, max: 85, ownership: "device" }
        humidity:     { unit: "%", min: 0, max: 100, ownership: "device" }
        co2:          { unit: "ppm", min: 400, max: 5000, ownership: "device" }
        interval:     { unit: "ms", min: 1000, max: 3600000, ownership: "cloud" }
```

Critical for LoRaWAN (raw bytes need decoding) and for normalizing heterogeneous device data into a consistent schema.

### Geolocation / Spatial

```sql
-- PostGIS extension in app Postgres
ALTER TABLE devices ADD COLUMN location GEOMETRY(POINT, 4326);

-- Spatial queries
SELECT thing_name, ST_Distance(location, ST_MakePoint(-122.4, 37.7))
FROM devices
WHERE ST_DWithin(location, ST_MakePoint(-122.4, 37.7), 500)
ORDER BY ST_Distance(location, ST_MakePoint(-122.4, 37.7));

-- Geofencing
SELECT thing_name FROM devices
WHERE ST_Within(location, ST_GeomFromGeoJSON('{...geofence polygon...}'));
```

### Offline / Store-and-Forward

- Gateway buffers telemetry locally when cloud is unreachable
- Configurable buffer depth (hours/days/messages)
- On reconnect: batch upload, not one publish per reading
- TimescaleDB handles out-of-order inserts natively
- Clock drift: device timestamps validated, adjusted if necessary
- Compression: CBOR or Protobuf for wire format, batch multiple readings per MQTT publish
