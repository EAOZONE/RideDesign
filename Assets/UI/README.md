# Ride Control MQTT System — Setup Guide

This document describes the required setup steps for running the **MQTT-based ride control system**, including the broker configuration, environment setup, and how to verify that all components (controller, sensors, vehicles, and dashboard UI) can communicate.

The system uses **Eclipse Mosquitto** as the MQTT broker and supports both:

* Standard MQTT connections (port **1883**) for controllers, microcontrollers, and Python scripts.
* **WebSocket MQTT** (port **9001**) for the browser-based dashboard UI.

---

# 1. Install Mosquitto

Download the Windows installer from the official site:

https://mosquitto.org/download/

Install the latest **64-bit Windows version**.

During installation ensure the following are enabled:

* Install Mosquitto Service
* Install Client Utilities

These provide the command-line tools:

```
mosquitto
mosquitto_pub
mosquitto_sub
```

---

# 2. Add Mosquitto to the System PATH

To allow the MQTT tools to run from any terminal, add the Mosquitto installation directory to the Windows PATH.

Typical install location:

```
C:\Program Files\mosquitto
```

### Steps

1. Open **Windows Search**
2. Search for **Environment Variables**
3. Select **Edit the system environment variables**
4. Click **Environment Variables**
5. Under **System Variables**, find **Path**
6. Click **Edit**
7. Click **New**
8. Add:

```
C:\Program Files\mosquitto
```

9. Click **OK** to save.

Restart PowerShell or the terminal after updating PATH.

---

# 3. Configure mosquitto.conf

The default configuration must be modified to enable both normal MQTT and WebSocket connections.

Open:

```
C:\Program Files\mosquitto\mosquitto.conf
```

Add or modify the following configuration:

```
listener 1883
allow_anonymous true

listener 9001
protocol websockets
```

### Explanation

| Setting              | Purpose                                                                         |
| -------------------- | ------------------------------------------------------------------------------- |
| listener 1883        | Standard MQTT port used by controllers, ESP32s, and Python clients              |
| allow_anonymous true | Allows devices to connect without authentication (simplifies local development) |
| listener 9001        | WebSocket port used by browser dashboards                                       |
| protocol websockets  | Enables MQTT communication over WebSockets                                      |

---

# 4. Start the MQTT Broker

Open PowerShell or Command Prompt and run:

```
mosquitto -v
```

You should see output similar to:

```
mosquitto version 2.x starting
Opening ipv4 listen socket on port 1883
Opening websockets listen socket on port 9001
```

The `-v` flag enables verbose logging which is useful for debugging.

---

# 5. Test MQTT Communication

Open two terminals.

### Terminal 1 (Subscriber)

```
mosquitto_sub -v -t "ride/#"
```

This subscribes to all ride-system topics.

---

### Terminal 2 (Publisher)

Send a test sensor message:

```
mosquitto_pub -t ride/sensor/Switch1/state -m "{\"state\":1}"
```

Expected output in Terminal 1:

```
ride/sensor/Switch1/state {"state":1}
```

This confirms the broker is functioning correctly.

---

# 6. WebSocket MQTT for the Dashboard

The ride dashboard connects using WebSockets.

Example connection code:

```javascript
const client = mqtt.connect("ws://localhost:9001");
```

Because WebSockets run on port **9001**, this requires the `protocol websockets` configuration in `mosquitto.conf`.

Without that configuration the dashboard will fail to connect.

---

# 7. Python Controller Setup

Install the required Python MQTT library:

```
pip install paho-mqtt
```

Example broker connection:

```python
BROKER = "localhost"
PORT = 1883
```

Python components communicate with the broker using **standard MQTT**, not WebSockets.

---

# 8. Topic Structure

The ride control system uses a structured topic hierarchy.

### Sensors

```
ride/sensor/<sensorID>/state
```

Example:

```
ride/sensor/Switch1/state
{"state":1}
```

---

### Vehicle Commands

```
ride/vehicle/<vehicleID>/drive/command
ride/vehicle/<vehicleID>/servoYaw/command
ride/vehicle/<vehicleID>/servoPitch/command
```

---

### Actuator Commands

```
ride/actuator/switchTrack/command
ride/actuator/rotateTrack/command
ride/actuator/dropTrack/command
```

---

### System Control

```
ride/system/mode
ride/system/estop
ride/system/reset
```

---

### Heartbeat

```
ride/controller/heartbeat
```

Published once per second to indicate the controller is running.

---

# 9. Useful Debug Commands

Subscribe to everything:

```
mosquitto_sub -v -t "#"
```

Watch only ride system messages:

```
mosquitto_sub -v -t "ride/#"
```

Send a test sensor trigger:

```
mosquitto_pub -t ride/sensor/Station1/state -m "{\"state\":1}"
```

---

# 10. System Architecture Overview

```
Sensors (ESP32 / Arduino)
        │
        │ MQTT
        ▼
     Mosquitto Broker
        │
        │
 ┌──────┴───────────┐
 │                  │
 ▼                  ▼
Ride Controller   Web Dashboard
(Python)          (JavaScript)
```

The broker acts as the **central message bus** connecting:

* sensors
* vehicles
* actuators
* control software
* visualization UI

---

# 11. Common Issues

### Dashboard cannot connect

Ensure WebSockets are enabled:

```
listener 9001
protocol websockets
```

---

### mosquitto command not found

Ensure the Mosquitto directory is in the system PATH.

---

### JSON errors

Messages must use valid JSON:

Correct:

```
{"state":1}
```

Incorrect:

```
{state:1}
```

---

# 12. Using HTML + Python
Run python code:

```python ride_control.py```

Open HTML file should open in a new tab

Utilizing the terminal type in different commands such as

```mosquitto_pub -h localhost -t ride/sensor/Switch1/state -m '{\"state\":1}'```

You should see the yellow dot travel around.

---

# 13. Recommended Debug Tool

To monitor the entire ride network in real time:

```
mosquitto_sub -v -t "ride/#"
```

This is extremely useful when debugging sensors, vehicles, and actuators.
