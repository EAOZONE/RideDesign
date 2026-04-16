import express from "express";
import { createServer as createViteServer } from "vite";
import path from "path";
import { spawn } from "child_process";
import Aedes from "aedes";
import { createServer as createNetServer } from "net";
import { createServer as createHttpServer } from "http";
import ws from "websocket-stream";

async function startServer() {
  const app = express();
  const PORT = 3000;

  // 1. Setup MQTT Broker (Aedes)
  const broker = new Aedes();
  
  // TCP Server for Python/ESP32 (Port 1883)
  const tcpServer = createNetServer(broker.handle);
  tcpServer.listen(1883, "0.0.0.0", () => {
    console.log("MQTT Broker (TCP) running on port 1883");
  });

  // HTTP Server for Express and WebSockets (Port 3000)
  const httpServer = createHttpServer(app);

  // Attach Aedes to WebSockets on the same HTTP server
  // We'll use a specific path for MQTT over WS
  ws.createServer({ server: httpServer, path: "/mqtt" }, broker.handle as any);

  // 2. Spawn Ride Controller (Python)
  // Try 'python3' first, then 'python'
  let pythonCmd = "python3";
  const pythonProcess = spawn(pythonCmd, ["ride_controller.py"]);
  
  pythonProcess.on("error", (err) => {
    console.warn(`Failed to start with ${pythonCmd}, trying 'python'...`);
    const fallbackProcess = spawn("python", ["ride_controller.py"]);
    
    fallbackProcess.stdout.on("data", (data) => console.log(`[Python] ${data}`));
    fallbackProcess.stderr.on("data", (data) => console.error(`[Python Error] ${data}`));
    fallbackProcess.on("error", (e) => console.error("Could not start Python controller. Ensure Python is installed."));
  });

  pythonProcess.stdout.on("data", (data) => {
    console.log(`[Python] ${data}`);
  });

  pythonProcess.stderr.on("data", (data) => {
    console.error(`[Python Error] ${data}`);
  });

  pythonProcess.on("close", (code) => {
    console.log(`[Python] process exited with code ${code}`);
  });

  // 3. API Routes
  app.get("/api/health", (req, res) => {
    res.json({ status: "ok", mqtt: "running" });
  });

  // 4. Vite Middleware for Development
  if (process.env.NODE_ENV !== "production") {
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
    console.log(`MQTT WebSockets available at ws://localhost:${PORT}/mqtt`);
  });
}

startServer().catch((err) => {
  console.error("Failed to start server:", err);
});
