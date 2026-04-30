import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    testTimeout: 120_000, // 2 minutes — hardware tests are slow
    hookTimeout: 120_000,
    sequence: {
      sequential: true, // hardware is a shared resource
      sequencer: class {
        async shard() { return []; }
        async sort(files: any[]) {
          return files.sort((a: any, b: any) => a.moduleId.localeCompare(b.moduleId));
        }
      } as any,
    },
    // Run all test files in a single process — they share serial port + MQTT state
    pool: "forks",
    poolOptions: {
      forks: {
        singleFork: true,
      },
    },
    include: ["src/tests/main.test.ts"],
  },
});
