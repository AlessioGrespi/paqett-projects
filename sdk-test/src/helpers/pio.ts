import { execFile } from "node:child_process";
import { resolve } from "node:path";

const FIRMWARE_DIR = resolve(import.meta.dirname, "../../firmware");

function run(args: string[], cwd = FIRMWARE_DIR): Promise<string> {
  return new Promise((resolve, reject) => {
    const pioPath = process.env.PIO_PATH ?? `${process.env.HOME}/.platformio/penv/bin/pio`;
    execFile(pioPath, args, { cwd, timeout: 300_000 }, (err, stdout, stderr) => {
      if (err) {
        reject(new Error(`pio ${args.join(" ")} failed:\n${stderr}\n${stdout}`));
      } else {
        resolve(stdout);
      }
    });
  });
}

/** Build the test firmware. */
export async function build(): Promise<string> {
  return run(["run"]);
}

/** Build and flash the test firmware to the connected ESP32. */
export async function flash(): Promise<string> {
  return run(["run", "-t", "upload"]);
}

/** Build the firmware with a custom version string (for OTA testing). */
export async function buildWithVersion(version: string): Promise<string> {
  const pioPath = process.env.PIO_PATH ?? `${process.env.HOME}/.platformio/penv/bin/pio`;
  return new Promise((resolve, reject) => {
    execFile(pioPath, ["run"], {
      cwd: FIRMWARE_DIR,
      timeout: 300_000,
      env: { ...process.env, PLATFORMIO_BUILD_FLAGS: `-DPAQETT_FIRMWARE_VERSION=\\"${version}\\"` },
    }, (err, stdout, stderr) => {
      if (err) reject(new Error(`pio run (v${version}) failed:\n${stderr}\n${stdout}`));
      else resolve(firmwareBinPath());
    });
  });
}

/** Get the path to the built firmware binary. */
export function firmwareBinPath(): string {
  return resolve(FIRMWARE_DIR, ".pio/build/esp32s3/firmware.bin");
}
