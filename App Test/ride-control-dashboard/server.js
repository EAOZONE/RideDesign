import express from "express";
import path from "path";
import { spawn } from "child_process";
import { createServer as createHttpServer } from "http";

async function startServer() {
  const app = express();
  const PORT = 3000;
  const isProduction = process.env.NODE_ENV === "production" || process.argv.includes("--production");
  const mqttHost = process.env.MQTT_HOST || "localhost";
  const mqttTcpPort = Number(process.env.MQTT_TCP_PORT || 1883);
  const mqttWsPort = Number(process.env.MQTT_WS_PORT || 9001);
  const httpServer = createHttpServer(app);

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
        mode: "external",
        host: mqttHost,
        tcpPort: mqttTcpPort,
        wsPort: mqttWsPort,
      },
    });
  });

  // 2. Vite Middleware for Development
  if (!isProduction) {
    const { createServer: createViteServer } = await import("vite");
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: "spa",
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(process.cwd(), "dist");
    app.use(express.static(distPath));
    app.get("*", (req, res) => {
      res.sendFile(path.join(distPath, "index.html"));
    });
  }

  httpServer.listen(PORT, "0.0.0.0", () => {
    console.log(`Server running on http://localhost:${PORT}`);
    console.log(`Using external MQTT broker at mqtt://${mqttHost}:${mqttTcpPort}`);
    console.log(`Using external MQTT WebSockets at ws://${mqttHost}:${mqttWsPort}`);
  });
}

startServer().catch((err) => {
  console.error("Failed to start server:", err);
});
