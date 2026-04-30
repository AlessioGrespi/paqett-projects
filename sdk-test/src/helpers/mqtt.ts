import mqtt, { type MqttClient } from "mqtt";

const BROKER_URL = process.env.MQTT_BROKER_URL ?? "mqtt://localhost:1883";

/**
 * Test MQTT client — connects to EMQX on plain TCP as "paqett-test-runner".
 * The paqett-ingest client ID has full ACL access to devices/# topics,
 * but we use a unique ID to avoid conflicts with the actual ingest worker.
 * The ACL allows any client to publish/subscribe to devices/${clientid}/# topics,
 * and the ingest worker has wildcard access. For testing, we subscribe via
 * the ingest-compatible topics.
 */
export class TestMqtt {
  private client: MqttClient | null = null;
  private messageHandlers: Map<string, Array<(payload: Record<string, unknown>) => void>> = new Map();

  async connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      // Use paqett-ingest client ID for full ACL access to all device topics
      this.client = mqtt.connect(BROKER_URL, {
        clientId: "paqett-test-runner",
        clean: true,
      });
      this.client.setMaxListeners(50);

      this.client.on("connect", () => {
        this.client!.on("message", (topic, message) => {
          const handlers = this.messageHandlers.get(topic);
          if (!handlers) return;
          try {
            const payload = JSON.parse(message.toString());
            for (const h of handlers) h(payload);
          } catch {
            // ignore non-JSON
          }
        });
        resolve();
      });

      this.client.on("error", reject);
    });
  }

  async disconnect(): Promise<void> {
    return new Promise((resolve) => {
      if (!this.client) return resolve();
      this.client.end(false, () => resolve());
    });
  }

  /** Subscribe to a topic and register a handler. */
  subscribe(topic: string, handler: (payload: Record<string, unknown>) => void): void {
    if (!this.client) throw new Error("MQTT not connected");
    const handlers = this.messageHandlers.get(topic) ?? [];
    handlers.push(handler);
    this.messageHandlers.set(topic, handlers);
    this.client.subscribe(topic);
  }

  /** Publish a JSON payload to a topic. */
  publish(topic: string, payload: Record<string, unknown>): void {
    if (!this.client) throw new Error("MQTT not connected");
    this.client.publish(topic, JSON.stringify(payload), { qos: 1 });
  }

  /** Clear a retained message by publishing empty payload with retain flag. */
  clearRetained(topic: string): void {
    if (!this.client) throw new Error("MQTT not connected");
    this.client.publish(topic, "", { qos: 1, retain: true });
  }

  /** Publish a command to a device. */
  publishCommand(
    thingName: string,
    command: string,
    id: string,
    params: Record<string, unknown> = {}
  ): void {
    this.publish(`devices/${thingName}/commands`, { command, id, params });
  }

  /** Wait for a message on a topic matching a predicate. */
  waitForMessage(
    topic: string,
    predicate: (payload: Record<string, unknown>) => boolean,
    timeoutMs = 15_000
  ): Promise<Record<string, unknown>> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        cleanup();
        reject(new Error(`Timeout waiting for message on ${topic} (${timeoutMs}ms)`));
      }, timeoutMs);

      const handler = (payload: Record<string, unknown>) => {
        if (predicate(payload)) {
          clearTimeout(timer);
          cleanup();
          resolve(payload);
        }
      };

      const cleanup = () => {
        const handlers = this.messageHandlers.get(topic);
        if (handlers) {
          const idx = handlers.indexOf(handler);
          if (idx >= 0) handlers.splice(idx, 1);
        }
      };

      this.subscribe(topic, handler);
    });
  }

  /** Subscribe to telemetry and collect messages. */
  collectTelemetry(thingName: string, count: number, timeoutMs = 30_000): Promise<Array<Record<string, unknown>>> {
    const topic = `devices/${thingName}/telemetry`;
    const collected: Array<Record<string, unknown>> = [];

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        cleanup();
        reject(new Error(`Only received ${collected.length}/${count} telemetry messages (${timeoutMs}ms)`));
      }, timeoutMs);

      const handler = (payload: Record<string, unknown>) => {
        collected.push(payload);
        if (collected.length >= count) {
          clearTimeout(timer);
          cleanup();
          resolve(collected);
        }
      };

      const cleanup = () => {
        const handlers = this.messageHandlers.get(topic);
        if (handlers) {
          const idx = handlers.indexOf(handler);
          if (idx >= 0) handlers.splice(idx, 1);
        }
      };

      this.subscribe(topic, handler);
    });
  }

  /** Subscribe to command ACKs for a device. */
  waitForAck(thingName: string, commandId: string, timeoutMs = 15_000): Promise<Record<string, unknown>> {
    return this.waitForMessage(
      `devices/${thingName}/commands/ack`,
      (p) => p.id === commandId,
      timeoutMs
    );
  }
}
