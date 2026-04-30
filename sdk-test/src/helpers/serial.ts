import { SerialPort } from "serialport";
import { ReadlineParser } from "serialport";

// JSON line types emitted by the test firmware
export type TestResult = {
  type: "test";
  id: string;
  name: string;
  status: "pass" | "fail";
  error?: string;
};

export type ReadyEvent = {
  type: "ready";
  phase: string;
};

export type DeviceEvent = {
  type: "event";
  name: string;
  [key: string]: unknown;
};

export type WaitingEvent = {
  type: "waiting";
  for: string;
};

export type FirmwareLine = TestResult | ReadyEvent | DeviceEvent | WaitingEvent;

export type HostCommand = {
  action: string;
  [key: string]: unknown;
};

/**
 * Auto-detect the ESP32 serial port by looking for common USB-serial chips.
 */
export async function detectPort(): Promise<string> {
  const ports = await SerialPort.list();
  const esp = ports.find(
    (p) =>
      p.manufacturer?.toLowerCase().includes("silicon") || // CP2102
      p.manufacturer?.toLowerCase().includes("wch") || // CH340
      p.vendorId === "303A" || // Espressif USB-JTAG
      p.vendorId === "303a" ||
      p.path.includes("tty.usbmodem") ||
      p.path.includes("tty.usbserial") ||
      p.path.includes("ttyACM") ||
      p.path.includes("ttyUSB")
  );
  if (!esp) {
    const available = ports.map((p) => `${p.path} (${p.manufacturer ?? "unknown"})`).join(", ");
    throw new Error(`No ESP32 serial port found. Available: ${available || "none"}`);
  }
  return esp.path;
}

/**
 * Manages serial communication with the ESP32 test firmware.
 */
export class DeviceSerial {
  private port: SerialPort;
  private parser: ReadlineParser;
  private lines: FirmwareLine[] = [];
  private rawLines: string[] = [];
  private listeners: Array<(line: FirmwareLine) => void> = [];

  constructor(path: string, baudRate = 115200) {
    this.port = new SerialPort({ path, baudRate, autoOpen: false });
    this.parser = this.port.pipe(new ReadlineParser({ delimiter: "\n" }));

    this.parser.on("data", (raw: string) => {
      const trimmed = raw.trim();
      if (!trimmed) return;
      this.rawLines.push(trimmed);

      try {
        const parsed = JSON.parse(trimmed) as FirmwareLine;
        if (parsed.type) {
          this.lines.push(parsed);
          for (const cb of this.listeners) cb(parsed);
        }
      } catch {
        // Not JSON — debug output, ignore
      }
    });
  }

  async open(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.port.open((err) => {
        if (err) reject(err);
        else resolve();
      });
    });
  }

  /** Reset the ESP32 by toggling DTR/RTS lines. */
  async resetDevice(): Promise<void> {
    await this.port.set({ dtr: false, rts: true });
    await new Promise((r) => setTimeout(r, 100));
    await this.port.set({ dtr: true, rts: false });
    await new Promise((r) => setTimeout(r, 500));
    this.flush();
  }

  async close(): Promise<void> {
    return new Promise((resolve) => {
      if (!this.port.isOpen) return resolve();
      this.port.close(() => resolve());
    });
  }

  /** Send a JSON command to the firmware. */
  send(command: HostCommand): void {
    this.port.write(JSON.stringify(command) + "\n");
  }

  /** Wait for a specific test result by ID. */
  waitForTest(id: string, timeoutMs = 30_000): Promise<TestResult> {
    return this.waitFor(
      (line): line is TestResult => line.type === "test" && line.id === id,
      timeoutMs,
      `test ${id}`
    );
  }

  /** Wait for a device event by name. */
  waitForEvent(name: string, timeoutMs = 30_000): Promise<DeviceEvent> {
    return this.waitFor(
      (line): line is DeviceEvent => line.type === "event" && line.name === name,
      timeoutMs,
      `event ${name}`
    );
  }

  /** Wait for a "ready" signal for a specific phase. */
  waitForReady(phase: string, timeoutMs = 60_000): Promise<ReadyEvent> {
    return this.waitFor(
      (line): line is ReadyEvent => line.type === "ready" && line.phase === phase,
      timeoutMs,
      `ready:${phase}`
    );
  }

  /** Wait for the firmware to signal it's waiting for a specific input. */
  waitForWaiting(forWhat: string, timeoutMs = 30_000): Promise<WaitingEvent> {
    return this.waitFor(
      (line): line is WaitingEvent => line.type === "waiting" && line.for === forWhat,
      timeoutMs,
      `waiting:${forWhat}`
    );
  }

  /** Collect all events matching a predicate within a time window. */
  collectEvents(predicate: (line: FirmwareLine) => boolean, durationMs: number): Promise<FirmwareLine[]> {
    return new Promise((resolve) => {
      const collected: FirmwareLine[] = [];
      const cb = (line: FirmwareLine) => {
        if (predicate(line)) collected.push(line);
      };
      this.listeners.push(cb);
      setTimeout(() => {
        this.listeners = this.listeners.filter((l) => l !== cb);
        resolve(collected);
      }, durationMs);
    });
  }

  /** Clear buffered lines. */
  flush(): void {
    this.lines = [];
    this.rawLines = [];
  }

  /** Remove all buffered events matching a given name. Returns count removed. */
  drainEvents(name: string): number {
    const before = this.lines.length;
    this.lines = this.lines.filter((l) => !(l.type === "event" && (l as DeviceEvent).name === name));
    return before - this.lines.length;
  }

  /** Get all raw serial output (for debugging). */
  getRawOutput(): string[] {
    return [...this.rawLines];
  }

  private waitFor<T extends FirmwareLine>(
    predicate: (line: FirmwareLine) => line is T,
    timeoutMs: number,
    label: string
  ): Promise<T> {
    // Check already-buffered lines first
    const existing = this.lines.find(predicate);
    if (existing) {
      this.lines = this.lines.filter((l) => l !== existing);
      return Promise.resolve(existing);
    }

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.listeners = this.listeners.filter((l) => l !== cb);
        reject(new Error(`Timeout waiting for ${label} (${timeoutMs}ms)`));
      }, timeoutMs);

      const cb = (line: FirmwareLine) => {
        if (predicate(line)) {
          clearTimeout(timer);
          this.listeners = this.listeners.filter((l) => l !== cb);
          // Remove from buffer so it's not consumed twice
          this.lines = this.lines.filter((l) => l !== line);
          resolve(line);
        }
      };
      this.listeners.push(cb);
    });
  }
}
