import json
import time
from typing import Any

import paho.mqtt.client as mqtt

BROKER = "10.59.183.183"  # Change to your Mosquitto broker IP if needed.
PORT = 1883

TRACK_ACTUATOR_DEFAULTS = {
    "switchTrack": {"target_angle": 0},
    "rotateTrack": {"target_angle": 0},
    "dropTrack": {
        "target": "top",
        "motor_a_speed": 0,
        "motor_b_speed": 0,
    },
}

SENSOR_IDS = [
    "Station1",
    "Switch1",
    "Switch2",
    "Rotate1",
    "Drop1",
    "Station2",
]

VEHICLE_IDS = ["0"]

ride_mode = "manual"
estop_active = False

vehicles: dict[str, dict[str, Any]] = {
    vehicle_id: {
        "drive": {"speed": 0.0, "left_speed": 0, "right_speed": 0, "moving": False},
        "servoYaw": {"angle": 90},
        "servoPitch": {"angle": 90},
        "last_seen": 0.0,
    }
    for vehicle_id in VEHICLE_IDS
}

sensors: dict[str, int] = {sensor_id: 0 for sensor_id in SENSOR_IDS}

actuators: dict[str, dict[str, Any]] = {
    name: payload.copy() for name, payload in TRACK_ACTUATOR_DEFAULTS.items()
}

switch_waiting_for_alignment = False
switch_waiting_for_clear = False

client = mqtt.Client(client_id="ride_controller")


def publish_json(topic: str, payload: dict[str, Any]) -> None:
    print("TX:", topic, payload)
    client.publish(topic, json.dumps(payload))


def on_connect(client_obj, userdata, flags, rc):
    print("Connected to MQTT with code:", rc)

    client_obj.subscribe("ride/sensor/+/state")
    client_obj.subscribe("ride/vehicle/+/+/state")
    client_obj.subscribe("ride/actuator/+/state")
    client_obj.subscribe("ride/system/#")


def on_message(client_obj, userdata, msg):
    topic = msg.topic

    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
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


def handle_sensor(topic: str, data: dict[str, Any]) -> None:
    sensor_id = topic.split("/")[2]
    state = int(data.get("state", 0))

    sensors[sensor_id] = state
    print("Sensor", sensor_id, "=", state)

    if not estop_active:
        process_sensor(sensor_id, state)


def process_sensor(sensor_id: str, state: int) -> None:
    global switch_waiting_for_alignment
    global switch_waiting_for_clear

    if state != 1:
        return

    print("Triggered sensor:", sensor_id)

    if sensor_id == "Station1":
        drive_vehicle("0", speed=0.35)

    elif sensor_id == "Switch1":
        drive_vehicle("0", speed=0.0)
        command_switch_track(90)
        switch_waiting_for_alignment = True

    elif sensor_id == "Switch2" and switch_waiting_for_clear:
        command_switch_track(0)
        switch_waiting_for_clear = False

    elif sensor_id == "Rotate1":
        drive_vehicle("0", speed=0.0)
        command_turntable(90)

    elif sensor_id == "Drop1":
        drive_vehicle("0", speed=0.0)
        command_drop_track("bottom", motor_a_speed=180, motor_b_speed=180)
    elif sensor_id=="Drop2":
        command_drop_track("top", motor_a_speed=180, motor_b_speed=180)
    elif sensor_id == "Station2":
        drive_vehicle("0", speed=0.0)
        set_yaw("0", 90)
        set_pitch("0", 90)


def handle_vehicle_state(topic: str, data: dict[str, Any]) -> None:
    parts = topic.split("/")
    vehicle_id = parts[2]
    subsystem = parts[3]

    if vehicle_id not in vehicles:
        vehicles[vehicle_id] = {
            "drive": {},
            "servoYaw": {},
            "servoPitch": {},
            "last_seen": 0.0,
        }

    vehicles[vehicle_id][subsystem] = data
    vehicles[vehicle_id]["last_seen"] = time.time()
    print("Vehicle", vehicle_id, subsystem, data)


def handle_actuator_state(topic: str, data: dict[str, Any]) -> None:
    global switch_waiting_for_alignment
    global switch_waiting_for_clear

    actuator = topic.split("/")[2]
    actuators[actuator] = data
    print("Actuator", actuator, "state:", data)

    if actuator == "switchTrack":
        angle = data.get("angle")
        moving = data.get("moving", False)

        if switch_waiting_for_alignment and angle == 90 and not moving:
            drive_vehicle("0", speed=0.35)
            switch_waiting_for_alignment = False
            switch_waiting_for_clear = True

    elif actuator == "rotateTrack":
        angle = data.get("angle")
        moving = data.get("moving", False)

        if angle == 90 and not moving:
            drive_vehicle("0", speed=0.30)

    elif actuator == "dropTrack":
        target = data.get("target")
        moving = data.get("moving", False)

        if target == "bottom" and not moving:
            drive_vehicle("0", speed=0.25)


def handle_estop(data: dict[str, Any]) -> None:
    global estop_active

    estop_active = bool(data.get("active", False))

    if estop_active:
        print("!!! EMERGENCY STOP !!!")
        stop_all_vehicles()
        command_drop_track("hold", motor_a_speed=0, motor_b_speed=0)


def handle_mode(data: dict[str, Any]) -> None:
    global ride_mode

    ride_mode = data.get("mode", "manual")
    print("Ride mode:", ride_mode)


def handle_reset() -> None:
    global switch_waiting_for_alignment
    global switch_waiting_for_clear

    print("System reset")
    stop_all_vehicles()
    command_switch_track(0)
    command_turntable(0)
    command_drop_track("top", motor_a_speed=180, motor_b_speed=180)
    switch_waiting_for_alignment = False
    switch_waiting_for_clear = False


def drive_vehicle(
    vehicle_id: str,
    speed: float,
    left_speed: int | None = None,
    right_speed: int | None = None,
) -> None:
    if left_speed is None or right_speed is None:
        pwm = max(-255, min(255, int(speed * 255)))
        left_speed = pwm
        right_speed = pwm

    payload = {
        "speed": speed,
        "left_speed": left_speed,
        "right_speed": right_speed,
    }
    publish_json(f"ride/vehicle/{vehicle_id}/drive/command", payload)


def set_yaw(vehicle_id: str, angle: int) -> None:
    publish_json(
        f"ride/vehicle/{vehicle_id}/servoYaw/command",
        {"angle": angle},
    )


def set_pitch(vehicle_id: str, angle: int) -> None:
    publish_json(
        f"ride/vehicle/{vehicle_id}/servoPitch/command",
        {"angle": angle},
    )


def command_switch_track(target_angle: int) -> None:
    publish_json(
        "ride/actuator/switchTrack/command",
        {"target_angle": target_angle},
    )


def command_turntable(target_angle: int) -> None:
    publish_json(
        "ride/actuator/rotateTrack/command",
        {"target_angle": target_angle},
    )


def command_drop_track(target: str, motor_a_speed: int, motor_b_speed: int) -> None:
    publish_json(
        "ride/actuator/dropTrack/command",
        {
            "target": target,
            "motor_a_speed": motor_a_speed,
            "motor_b_speed": motor_b_speed,
        },
    )


def stop_all_vehicles() -> None:
    for vehicle_id in vehicles:
        drive_vehicle(vehicle_id, speed=0.0, left_speed=0, right_speed=0)


def send_heartbeat() -> None:
    publish_json(
        "ride/controller/heartbeat",
        {
            "alive": True,
            "mode": ride_mode,
            "estop": estop_active,
        },
    )


def main() -> None:
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT)
    client.loop_start()

    print("Ride controller running")

    last_heartbeat = 0.0

    while True:
        now = time.time()

        if now - last_heartbeat >= 1.0:
            send_heartbeat()
            last_heartbeat = now

        time.sleep(0.05)


if __name__ == "__main__":
    main()
