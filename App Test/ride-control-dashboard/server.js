import express from "express";
import path from "path";
import fs from "fs";
import { spawn } from "child_process";
import { createServer as createHttpServer } from "http";
import { createConnection, createServer as createMqttTcpServer } from "net";
import Aedes from "aedes";
import websocketStream from "websocket-stream";

async function startServer() {
  const app = express();
  const PORT = 3000;
  const isProduction = process.env.NODE_ENV === "production" || process.argv.includes("--production");
  const mqttHost = process.env.MQTT_HOST || "0.0.0.0";
  const mqttUpstreamHost = process.env.MQTT_UPSTREAM_HOST || "127.0.0.1";
  const mqttTcpPort = Number(process.env.MQTT_TCP_PORT || 1883);
  const httpServer = createHttpServer(app);
  const mqttBroker = Aedes();
  const mqttTcpServer = createMqttTcpServer(mqttBroker.handle);
  let mqttMode = "embedded";

  mqttBroker.on("clientError", (client, err) => {
    console.error(`[MQTT] client error ${client?.id || "unknown"}:`, err.message);
  });

  mqttBroker.on("connectionError", (client, err) => {
    console.error(`[MQTT] connection error ${client?.id || "unknown"}:`, err.message);
  });

  await new Promise((resolve, reject) => {
    mqttTcpServer.once("error", (err) => {
      if (err.code !== "EADDRINUSE") {
        reject(err);
        return;
      }

      mqttMode = "external";
      console.warn(
        `MQTT port ${mqttTcpPort} is already in use; using existing broker at mqtt://${mqttUpstreamHost}:${mqttTcpPort}`,
      );
      resolve();
    });

    mqttTcpServer.listen(mqttTcpPort, mqttHost, () => {
      console.log(`MQTT broker listening at mqtt://${mqttHost}:${mqttTcpPort}`);
      resolve();
    });
  });

  websocketStream.createServer(
    {
      server: httpServer,
      path: "/mqtt",
      perMessageDeflate: false,
    },
    (stream) => {
      if (mqttMode === "embedded") {
        mqttBroker.handle(stream);
        return;
      }

      const socket = createConnection(mqttTcpPort, mqttUpstreamHost);

      stream.pipe(socket).pipe(stream);

      socket.on("error", (err) => {
        console.error(`[MQTT] upstream connection error:`, err.message);
        stream.destroy(err);
      });
    },
  );

  // 1. Spawn Ride Controller (Python)
  let pythonCmd = "python3";
  const spawnPython = (cmd) => {
    const p = spawn(cmd, ["ride_controller.py"]);

    p.stdout.on("data", (data) => console.log(`[Python] ${data}`));
    p.stderr.on("data", (data) => console.error(`[Python Error] ${data}`));

    p.on("error", () => {
      if (cmd === "python3") {
        console.warn("Failed to start with python3, trying 'python'...");
        spawnPython("python");
      } else {
        console.error("Could not start Python controller. Ensure Python and paho-mqtt are installed.");
      }
    });

    p.on("close", (code) => {
      console.log(`[Python] process exited with code ${code}`);
    });
  };

  spawnPython(pythonCmd);

  // API Routes
  app.get("/api/health", (req, res) => {
    res.json({
      status: "ok",
      mqtt: {
        mode: mqttMode,
        host: mqttMode === "embedded" ? mqttHost : mqttUpstreamHost,
        tcpPort: mqttTcpPort,
        wsPath: "/mqtt",
      },
    });
  });

  // 2. Vite Middleware for Development
  if (!isProduction) {
    const { createServer: createViteServer } = await import("vite");
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: "custom", // Recommended for custom middleware setups
    });
    app.use(vite.middlewares);

    // Serve index.html in development
    app.use("*", async (req, res, next) => {
      const url = req.originalUrl;
      try {
        // 1. Read index.html
        let template = fs.readFileSync(path.resolve(process.cwd(), "index.html"), "utf-8");

        // 2. Apply Vite HTML transforms. This injects the Vite HMR client, and
        //    also applies HTML transforms from Vite plugins, e.g. global preambles
        //    from @vitejs/plugin-react
        template = await vite.transformIndexHtml(url, template);

        // 3. Send the rendered HTML back.
        res.status(200).set({ "Content-Type": "text/html" }).end(template);
      } catch (e) {
        // If an error is caught, let Vite fix the stack trace so it maps back to
        // your actual source code.
        vite.ssrFixStacktrace(e);
        next(e);
      }
    });
  } else {
    const distPath = path.join(process.cwd(), "dist");
    app.use(express.static(distPath));
    app.get("*", (req, res) => {
      res.sendFile(path.join(distPath, "index.html"));
    });
  }

  httpServer.listen(PORT, "0.0.0.0", () => {
    console.log(`Server running on http://localhost:${PORT}`);
    console.log(`MQTT WebSockets available at ws://localhost:${PORT}/mqtt`);
  });
}

startServer().catch((err) => {
  console.error("Failed to start server:", err);
});

