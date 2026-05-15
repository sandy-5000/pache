const HOST = "127.0.0.1";
const PORT = 5555;

const CONNECTIONS = 10;
const REQUESTS_PER_CONNECTION = 5;

const sendRequests = (sock: Bun.Socket<undefined>, clientId: number): void => {
  let sent = 0;

  const interval = setInterval(() => {
    if (sent >= REQUESTS_PER_CONNECTION) {
      clearInterval(interval);
      sock.end();
      return;
    }

    const message = `request-${clientId}-${sent}\n`;
    console.log(`client ${clientId} sending: ${message.trim()}`);
    sock.write(message);

    sent++;
  }, 20);
};

const onData = (clientId: number, data: Buffer | Uint8Array): void => {
  console.log(`client ${clientId} received:`, Buffer.from(data).toString());
};

const createClient = (clientId: number): Promise<void> => {
  return new Promise((resolve, reject) => {
    Bun.connect({
      hostname: HOST,
      port: PORT,

      socket: {
        open(sock) {
          console.log(`client ${clientId} connected`);
          sendRequests(sock, clientId);
        },

        data(sock, data) {
          onData(clientId, data);
        },

        close() {
          console.log(`client ${clientId} closed`);
          resolve();
        },

        error(_, error) {
          console.error(`client ${clientId} error`, error);
          reject(error);
        },
      },
    });
  });
};

const startTest = async (): Promise<void> => {
  const clients: Promise<void>[] = [];

  for (let i = 0; i < CONNECTIONS; i++) {
    clients.push(createClient(i));
  }

  await Promise.all(clients);
  console.log("all tests completed");
};

startTest();
