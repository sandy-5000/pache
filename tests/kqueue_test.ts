import { HOST, PORT } from "./constants";
/*
 * Tune these carefully.
 *
 * macOS default ulimit is usually low:
 *
 * ulimit -n
 *
 * Increase before running:
 *
 * ulimit -n 65536
 */

const TOTAL_CLIENTS = 1000;
const CONCURRENT_BATCH = 1000;
const REQUESTS_PER_CLIENT = 1000;
const REQUEST_INTERVAL_MS = 1;

// const CLIENT_LIFETIME_MS = 5000;

let connected = 0;
let completed = 0;
let failed = 0;
let messagesSent = 0;
let messagesReceived = 0;

const sleep = (ms: number): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, ms));

const createPayload = (clientId: number, requestId: number): string => {
  return (
    JSON.stringify({
      clientId,
      requestId,
      ts: Date.now(),
      data: "x".repeat(128),
    }) + "\n"
  );
};

const createKqueueClient = async (clientId: number): Promise<void> => {
  return new Promise((resolve) => {
    let requestsSent = 0;
    let responsesReceived = 0;

    let closed = false;

    const socket = Bun.connect({
      hostname: HOST,
      port: PORT,

      socket: {
        open(sock) {
          connected++;

          for (let i = 0; i < REQUESTS_PER_CLIENT; i++) {
            const payload = createPayload(clientId, i);

            if (sock.write(payload)) {
              messagesSent++;
              requestsSent++;
            }
          }
        },

        data(sock, data) {
          messagesReceived += data.length;
          const text = data.toString();

          for (const ch of text) {
            if (ch === "\n") {
              responsesReceived++;
            }
          }
          if (responsesReceived >= REQUESTS_PER_CLIENT) {
            sock.end();
          }
        },

        close() {
          if (!closed) {
            closed = true;
            completed++;
            resolve();
          }
        },

        error(_, err) {
          if (!closed) {
            closed = true;
            failed++;

            console.error(`client ${clientId} error`, err.message);

            resolve();
          }
        },
      },
    });

    if (!socket) {
      failed++;
      resolve();
    }
  });
};

const printStats = (): void => {
  const mem = process.memoryUsage();

  console.log(`
========================================
connected:         ${connected}
completed:         ${completed}
failed:            ${failed}

messages sent:     ${messagesSent}
messages received: ${messagesReceived}

rss:               ${(mem.rss / 1024 / 1024).toFixed(2)} MB
heap used:         ${(mem.heapUsed / 1024 / 1024).toFixed(2)} MB
========================================
`);
};

const startLoadTest = async (): Promise<void> => {
  console.log(`
Starting kqueue stress test

total clients:      ${TOTAL_CLIENTS}
concurrent batch:   ${CONCURRENT_BATCH}
requests/client:    ${REQUESTS_PER_CLIENT}
`);

  const statsInterval = setInterval(printStats, 1000);

  const active: Promise<void>[] = [];

  for (let i = 0; i < TOTAL_CLIENTS; i++) {
    active.push(createKqueueClient(i));

    /*
     * batch throttle
     */
    if (active.length >= CONCURRENT_BATCH) {
      await Promise.all(active);
      active.length = 0;

      /*
       * tiny breathing room
       */
      await sleep(10);
    }
  }

  if (active.length > 0) {
    await Promise.all(active);
  }

  clearInterval(statsInterval);

  printStats();

  console.log("load test completed");
};

startLoadTest().catch(console.error);
