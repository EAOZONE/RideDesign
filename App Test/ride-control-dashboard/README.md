# Ride Control System - Local Setup Guide

This guide explains how to run the Ride Control Dashboard and Controller on your **Mac** or **Raspberry Pi**.

## 1. Prerequisites

Ensure you have the following installed:

*   **Node.js** (v18 or higher)
*   **Python 3**
*   **pip** (Python package manager)

## 2. Installation

1.  **Clone or Download** this project to your local machine.
2.  **Install Node.js Dependencies**:
    ```bash
    npm install
    ```
3.  **Install Python Dependencies**:
    ```bash
    pip install paho-mqtt
    ```

## 3. Running the Application

The application is designed to run both the web server and the ride controller simultaneously.

1.  **Start the Development Server**:
    ```bash
    npm run dev
    ```
    This command will:
    *   Start the **MQTT Broker** on port 1883, or reuse an existing broker if port 1883 is already occupied.
    *   Expose MQTT WebSockets at `/mqtt` on port 3000 for the browser dashboard.
    *   Launch the **Python Ride Controller** (`ride_controller.py`) in the background.
    *   Start the **Vite Development Server** for the React UI.

2.  **Access the Dashboard**:
    Open your browser and navigate to:
    `http://localhost:3000`

## 4. Raspberry Pi Zero W (Production Mode)

The Pi Zero W has limited resources and may struggle with the full development environment. For the best experience:

1.  **Build the project** on a more powerful machine (Mac/PC):
    ```bash
    npm install
    npm run build
    ```
2.  **Transfer the project** (including the `dist` folder and `server.js`) to the Pi.
3.  **Install only production dependencies** on the Pi:
    ```bash
    npm install --omit=dev
    ```
4.  **Start the production server**:
    ```bash
    npm start
    ```

## 5. Connecting Hardware (ESP32 / Arduino)

Your hardware boards should connect to the IP address of your Mac/Raspberry Pi on **Port 1883**.

*   **MQTT Broker IP**: The local IP address of your computer (e.g., `192.168.1.50`).
*   **MQTT Port**: `1883`
*   **Protocol**: Standard MQTT (TCP).

## 5. Troubleshooting the Preview

If the preview in AI Studio is not loading:
1.  **Check the Terminal**: Look for any errors in the console output.
2.  **Python Path**: If `python3` is not in your path, the server might fail to start the controller.
3.  **Port Conflicts**: Ensure port `3000` and `1883` are not being used by other applications (like a standalone Mosquitto broker).

## 6. Track Layout
The dashboard now uses the **1000x1000 Grid Blueprint** with the following zones:
*   **Station**: Bottom-center
*   **Football**: Middle-left
*   **Basketball**: Center
*   **Baseball**: Top-right (includes the Hairpin Switch Track)
*   **Loop-de-loop**: Bottom-right quadrant
