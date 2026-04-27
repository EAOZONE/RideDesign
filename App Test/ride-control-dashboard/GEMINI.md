# Ride Control System - Project Context & Instructions

This guide provides core context, architecture details, and setup instructions for the Ride Control System. When assisting with this project, adhere to the network configurations, topic structures, and track layouts defined below.

## 1. System Architecture Overview

The system utilizes an MQTT broker as the central message bus connecting hardware sensors, Python-based control logic, and a web-based dashboard visualization. 

**Deployment Environment:**
* **Hardware:** Raspberry Pi (Main Controller).
* **Network:** Private, offline local network (No internet access).
* **Access Model:** The Pi runs the backend (Broker, Python Controller, Web Server). The UI is accessed via a browser on a separate computer/tablet connected to the same local network using the Pi's IP address.

    Sensors & Actuators (ESP32 / Arduino)
            │
            │ MQTT (Port 1883)
            ▼
         Mosquitto Broker (Local Network)
            │
            ├─────────────────────────────┐
            │                             │
            ▼                             ▼
    Ride Controller                 Web Dashboard
    (Python `paho-mqtt`)            (React/Vite or HTML via WebSockets)
    (Runs on Pi: localhost:1883)    (Accessed via Pi IP: http://<PI_IP>:3000)

## 2. Network & Broker Configuration

The system uses **Eclipse Mosquitto** and requires both standard TCP and WebSocket connections.

* **Standard MQTT (TCP):** Port `1883`. Used by ESP32/Arduinos, Python scripts (`ride_controller.py`), and backend controllers.
* **WebSocket MQTT:** Port `9001` (or proxied via Web Server on Port 3000). Used exclusively by the browser-based dashboard UI.

**Required `mosquitto.conf` setup:**
    listener 1883
    allow_anonymous true
    
    listener 9001
    protocol websockets

**Finding the Raspberry Pi IP:**
Run `hostname -I` on the Pi to find its local IP address (e.g., `192.168.1.XX`).

**Custom Local Domains (e.g., http://ride.dev):**
*   **Option A (Automatic):** Change the Pi hostname to `ride` (`sudo hostnamectl set-hostname ride`). You can then access it at `http://ride.local:3000` from any device with mDNS support (Mac, iOS, Windows).
*   **Option B (Custom .dev):** Edit the `hosts` file on your controlling computer (Mac: `/etc/hosts`, Windows: `C:\Windows\System32\drivers\etc\hosts`) and add: `<PI_IP> ride.dev`.

## 3. MQTT Topic Structure
... (rest of the file)

All communications must follow this structured topic hierarchy. Pay close attention to JSON payloads (e.g., `{"state":1}`).

### Sensors
* `ride/sensor/<sensorID>/state` (e.g., `ride/sensor/Switch1/state`)

### Vehicle Commands
* `ride/vehicle/<vehicleID>/drive/command`
* `ride/vehicle/<vehicleID>/servoYaw/command`
* `ride/vehicle/<vehicleID>/servoPitch/command`

### Actuator Commands
* `ride/actuator/switchTrack/command`
* `ride/actuator/rotateTrack/command`
* `ride/actuator/dropTrack/command`

### System Control & Status
* `ride/system/mode`
* `ride/system/estop`
* `ride/system/reset`
* `ride/controller/heartbeat` (Published 1x/sec by Python controller)

## 4. Setup & Running the Application

### Method A: Full Stack (Mac / Raspberry Pi / Node Environment)
1. Install dependencies: `npm install` and `pip install paho-mqtt`
2. Start development server: `npm run dev`
   * *Note: This automatically starts a local MQTT broker (1883/3000), launches the Python Ride Controller, and starts the Vite React UI.*
3. Access UI at `http://localhost:3000`

### Method B: Manual / Windows Environment
1. Install **Eclipse Mosquitto** (ensure it's added to system PATH).
2. Start broker: `mosquitto -v`
3. Run Python controller: `python ride_control.py` (or `ride_controller.py`)
4. Open the standalone HTML dashboard file in a browser.

## 5. Track Layout & Grid Blueprint
The dashboard UI utilizes a **1000x1000 Grid Blueprint** mapped to specific zones:
* **Station:** Bottom-center
* **Football:** Middle-left
* **Basketball:** Center
* **Baseball:** Top-right (includes the Hairpin Switch Track)
* **Loop-de-loop:** Bottom-right quadrant

## 6. Debugging Commands
Use these commands to monitor or simulate the network:
* **Subscribe to all ride messages:** `mosquitto_sub -v -t "ride/#"`
* **Simulate a sensor trigger:** `mosquitto_pub -h localhost -t ride/sensor/Switch1/state -m "{\"state\":1}"`


## 7. CRITICAL AGENT BEHAVIOR: Single-File Operations Only
Due to a known UI bug in the current extension build, **you must NEVER output more than one file operation (`WriteFile`, `Edit`, etc.) per response.** If a prompt requires you to create, edit, or read multiple files:
1. Perform the operation on the **FIRST** file only.
2. Stop generating immediately.
3. Add a text note saying: *"I have updated [File 1]. Say 'continue' to process the next file."*
4. Wait for the user's prompt before moving to the next file.

"Do not defer to sub-agents or the generalist agent. Process this request in the main session."

The password to the raspberry pi is "TPED" you can use then when sshing