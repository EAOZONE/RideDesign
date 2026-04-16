import express from "express";
import path from "path";
import { spawn } from "child_process";
import { Aedes } from "aedes";
import { createServer as createNetServer } from "net";
import { createServer as createHttpServer } from "http";
import ws from "websocket-stream";

async function startServer() {
  const app = express();
  const PORT = 3000;

  // 1. Setup MQTT Broker (Aedes)
  const broker = await Aedes.createBroker();
  
  // TCP Server for Python/ESP32 (Port 1883)
  const tcpServer = createNetServer(broker.handle);
  tcpServer.listen(1883, "0.0.0.0", () => {
    console.log("MQTT Broker (TCP) running on port 1883");
    
    // 2. Spawn Ride Controller (Python) - ONLY after broker is ready
    let pythonCmd = "python3";
    const spawnPython = (cmd) => {
      const p = spawn(cmd, ["ride_controller.py"]);
      
      p.stdout.on("data", (data) => console.log(`[Python] ${data}`));
      p.stderr.on("data", (data) => console.error(`[Python Error] ${data}`));
      
      p.on("error", (err) => {
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
      
      return p;
    };

    spawnPython(pythonCmd);
  });

  // 3. HTTP Server for Express and WebSockets (Port 3000)
  const httpServer = createHttpServer(app);

  // Attach Aedes to WebSockets on the same HTTP server
  ws.createServer({ server: httpServer, path: "/mqtt" }, broker.handle);

  // API Routes
  app.get("/api/health", (req, res) => {
    res.json({ status: "ok", mqtt: "running" });
  });

  // 4. Vite Middleware for Development
  if (process.env.NODE_ENV !== "production") {
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
    console.log(`MQTT WebSockets available at ws://localhost:${PORT}/mqtt`);
  });
}

startServer().catch((err) => {
  console.error("Failed to start server:", err);
});
