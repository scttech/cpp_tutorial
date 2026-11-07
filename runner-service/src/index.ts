import Docker from "dockerode";
import { config } from "./config.js";
import { DockerSandbox } from "./sandbox.js";
import { buildServer } from "./server.js";

// DOCKER_HOST (e.g. tcp://socket-proxy:2375) is honoured by dockerode via docker-modem.
const docker = new Docker();
const app = await buildServer({ sandbox: new DockerSandbox(docker, config), config });

const stop = async () => {
  await app.close();
  process.exit(0);
};
process.on("SIGTERM", stop);
process.on("SIGINT", stop);

await app.listen({ port: config.port, host: config.host });
