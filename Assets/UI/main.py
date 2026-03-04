import json
import time
import paho.mqtt.client as mqtt

BROKER = "10.51.222.143"
PORT = 1883

# -----------------------------
# GLOBAL STATE
# -----------------------------

ride_mode = "manual"
estop_active = False

vehicles = {}
sensors = {}

# -----------------------------
# MQTT CLIENT
# -----------------------------

client = mqtt.Client(client_id="ride_controller")

# -----------------------------
# CONNECT
# -----------------------------

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT with code:", rc)

    # Sensors
    client.subscribe("ride/sensor/+/state")

    # Vehicle state
    client.subscribe("ride/vehicle/+/drive/state")
    client.subscribe("ride/vehicle/+/servoYaw/state")
    client.subscribe("ride/vehicle/+/servoPitch/state")

    # System control
    client.subscribe("ride/system/#")

    # Actuator state
    client.subscribe("ride/actuator/+/state")


# -----------------------------
# MESSAGE RECEIVED
# -----------------------------

def on_message(client, userdata, msg):

    topic = msg.topic

    try:
        data = json.loads(msg.payload.decode())
    except:
        print("Bad JSON:", msg.payload)
        return

    print("RX:", topic, data)

    if topic.startswith("ride/sensor/"):
        handle_sensor(topic, data)

    elif topic.startswith("ride/vehicle/"):
        handle_vehicle_state(topic, data)

    elif topic.startswith("ride/actuator/"):
        handle_actuator_state(topic, data)

    elif topic == "ride/system/estop":
        handle_estop(data)

    elif topic == "ride/system/mode":
        handle_mode(data)

    elif topic == "ride/system/reset":
        handle_reset()


# -----------------------------
# SENSOR HANDLING
# -----------------------------

def handle_sensor(topic, data):

    sensor_id = topic.split("/")[2]
    state = data["state"]

    sensors[sensor_id] = state

    print("Sensor", sensor_id, "=", state)

    if ride_mode == "automatic" and not estop_active:
        process_sensor(sensor_id, state)


# -----------------------------
# SENSOR LOGIC
# -----------------------------

def process_sensor(sensor_id, state):

    if state != 1:
        return

    print("Triggered sensor:", sensor_id)

    # Example logic

    if sensor_id == "1":

        print("Switch track")

        client.publish(
            "ride/actuator/switchTrack/command",
            json.dumps({"angle":90})
        )

    if sensor_id == "2":

        print("Rotate track")

        client.publish(
            "ride/actuator/rotateTrack/command",
            json.dumps({"angle":90})
        )

    if sensor_id == "3":

        print("Drop track")

        client.publish(
            "ride/actuator/dropTrack/command",
            json.dumps({"position":"bottom"})
        )


# -----------------------------
# VEHICLE STATE
# -----------------------------

def handle_vehicle_state(topic, data):

    parts = topic.split("/")
    vehicle_id = parts[2]

    if vehicle_id not in vehicles:
        vehicles[vehicle_id] = {}

    vehicles[vehicle_id].update(data)

    print("Vehicle", vehicle_id, vehicles[vehicle_id])


# -----------------------------
# ACTUATOR STATE
# -----------------------------

def handle_actuator_state(topic, data):

    actuator = topic.split("/")[2]

    print("Actuator", actuator, "state:", data)


# -----------------------------
# SYSTEM COMMANDS
# -----------------------------

def handle_estop(data):

    global estop_active

    estop_active = data["active"]

    if estop_active:
        print("!!! EMERGENCY STOP !!!")
        stop_all_vehicles()


def handle_mode(data):

    global ride_mode

    ride_mode = data["mode"]

    print("Ride mode:", ride_mode)


def handle_reset():

    print("System reset")

    stop_all_vehicles()


# -----------------------------
# VEHICLE COMMANDS
# -----------------------------

def drive_vehicle(vehicle_id, speed):

    topic = f"ride/vehicle/{vehicle_id}/drive/command"

    payload = {
        "speed": speed
    }

    client.publish(topic, json.dumps(payload))


def set_yaw(vehicle_id, angle):

    topic = f"ride/vehicle/{vehicle_id}/servoYaw/command"

    payload = {
        "angle": angle
    }

    client.publish(topic, json.dumps(payload))


def set_pitch(vehicle_id, angle):

    topic = f"ride/vehicle/{vehicle_id}/servoPitch/command"

    payload = {
        "angle": angle
    }

    client.publish(topic, json.dumps(payload))


# -----------------------------
# SAFETY
# -----------------------------

def stop_all_vehicles():

    for vehicle_id in vehicles:
        drive_vehicle(vehicle_id, 0)


# -----------------------------
# HEARTBEAT
# -----------------------------

def send_heartbeat():

    client.publish(
        "ride/controller/heartbeat",
        json.dumps({"alive":True})
    )


# -----------------------------
# MAIN LOOP
# -----------------------------

def main():

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT)

    client.loop_start()

    print("Ride controller running")

    last_heartbeat = time.time()

    while True:

        now = time.time()

        if now - last_heartbeat > 1:
            send_heartbeat()
            last_heartbeat = now

        time.sleep(0.05)


# -----------------------------
# START
# -----------------------------

if __name__ == "__main__":
    main()