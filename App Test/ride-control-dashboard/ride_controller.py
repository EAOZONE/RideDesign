import json
import threading
import time
from typing import Any, Callable

import paho.mqtt.client as mqtt

BROKER = "127.0.0.1"        # Local Development
# BROKER = "10.59.183.183"  # Remote Machine (Ben's IP)
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
    "Drop2",
    "Station2",
]

VEHICLE_IDS = ["0"]

SAVED_RIDE_ANGLE_PROFILES: dict[str, list[dict[str, int | str]]] = {
    "default": [
        {"sensor_id": "Station1", "yaw": 90},
        {"sensor_id": "Switch1", "yaw": 120},
        {"sensor_id": "Switch2", "yaw": 60},
        {"sensor_id": "Rotate1", "yaw": 90},
        {"sensor_id": "Drop1", "yaw": 90},
        {"sensor_id": "Drop2", "yaw": 90},
        {"sensor_id": "Station2", "yaw": 90},
    ],
}

ACTIVE_RIDE_PROFILE = "default"

SWITCH_TRACK_DIVERGE_ANGLE = 90
SWITCH_TRACK_REVERSE_SPEED = -90

STATION_TO_SWITCH_TIMED_SEQUENCE = [
    {"delay": 0.0, "speed": 90, "yaw": 90},
    {"delay": 1.0, "yaw": 120},
    {"delay": 2.5, "yaw": 60},
]

ride_mode = "manual"
estop_active = False

vehicles: dict[str, dict[str, Any]] = {
    vehicle_id: {
        "drive": {"speed": 0, "left_speed": 0, "right_speed": 0, "moving": False},
        "servoYaw": {"angle": 90},
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
scheduled_actions: list[tuple[float, Callable[[], None]]] = []
scheduled_actions_lock = threading.Lock()

# For paho-mqtt 2.0 compatibility, we specify callback_api_version
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)


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

    if not estop_active and ride_mode == "auto":
        process_sensor(sensor_id, state)


def process_sensor(sensor_id: str, state: int) -> None:
    global switch_waiting_for_alignment
    global switch_waiting_for_clear

    if state != 1:
        return

    print("Triggered sensor:", sensor_id)
    apply_saved_ride_angles("0", sensor_id)

    if sensor_id == "Station1":
        schedule_station_to_switch_sequence("0")

    elif sensor_id == "Switch1":
        drive_vehicle("0", speed=0)
        command_switch_track(SWITCH_TRACK_DIVERGE_ANGLE)
        switch_waiting_for_alignment = True

    elif sensor_id == "Switch2" and switch_waiting_for_clear:
        command_switch_track(0)
        switch_waiting_for_clear = False

    elif sensor_id == "Rotate1":
        drive_vehicle("0", speed=0)
        command_turntable(90)

    elif sensor_id == "Drop1":
        drive_vehicle("0", speed=0)
        command_drop_track("bottom", motor_a_speed=180, motor_b_speed=180)
    elif sensor_id == "Drop2":
        drive_vehicle("0", speed=255)
        command_drop_track("top", motor_a_speed=180, motor_b_speed=180)

    elif sensor_id == "Station2":
        drive_vehicle("0", speed=0)


def apply_saved_ride_angles(vehicle_id: str, sensor_id: str) -> None:
    for waypoint in SAVED_RIDE_ANGLE_PROFILES.get(ACTIVE_RIDE_PROFILE, []):
        if waypoint.get("sensor_id") != sensor_id:
            continue

        yaw = int(waypoint["yaw"])
        set_yaw(vehicle_id, yaw)
        print(
            "Applied saved ride angles:",
            ACTIVE_RIDE_PROFILE,
            sensor_id,
            f"yaw={yaw}",
        )
        return


def schedule_action(delay_seconds: float, action: Callable[[], None]) -> None:
    run_at = time.time() + delay_seconds
    with scheduled_actions_lock:
        scheduled_actions.append((run_at, action))


def clear_scheduled_actions() -> None:
    with scheduled_actions_lock:
        scheduled_actions.clear()


def process_scheduled_actions() -> None:
    now = time.time()

    with scheduled_actions_lock:
        due_actions = [
            scheduled_action
            for scheduled_action in scheduled_actions
            if scheduled_action[0] <= now
        ]
        pending_actions = [
            scheduled_action
            for scheduled_action in scheduled_actions
            if scheduled_action[0] > now
        ]
        scheduled_actions[:] = pending_actions

    for _, action in sorted(due_actions, key=lambda scheduled_action: scheduled_action[0]):
        if estop_active or ride_mode != "auto":
            continue

        action()


def schedule_station_to_switch_sequence(vehicle_id: str) -> None:
    clear_scheduled_actions()

    for step in STATION_TO_SWITCH_TIMED_SEQUENCE:
        delay = float(step["delay"])

        def run_step(sequence_step=step) -> None:
            if "speed" in sequence_step:
                drive_vehicle(vehicle_id, speed=int(sequence_step["speed"]))

            if "yaw" in sequence_step:
                set_yaw(vehicle_id, int(sequence_step["yaw"]))

        schedule_action(delay, run_step)

    print("Scheduled station-to-switch timed sequence")


def handle_vehicle_state(topic: str, data: dict[str, Any]) -> None:
    parts = topic.split("/")
    vehicle_id = parts[2]
    subsystem = parts[3]

    if vehicle_id not in vehicles:
        vehicles[vehicle_id] = {
            "drive": {},
            "servoYaw": {},
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

        if (
            switch_waiting_for_alignment
            and angle == SWITCH_TRACK_DIVERGE_ANGLE
            and not moving
        ):
            drive_vehicle("0", speed=SWITCH_TRACK_REVERSE_SPEED)
            switch_waiting_for_alignment = False
            switch_waiting_for_clear = True

    elif actuator == "rotateTrack":
        angle = data.get("angle")
        moving = data.get("moving", False)

        if angle == 90 and not moving:
            drive_vehicle("0", speed=75)

    elif actuator == "dropTrack":
        target = data.get("target")
        moving = data.get("moving", False)

        if target == "bottom" and not moving:
            drive_vehicle("0", speed=65)


def handle_estop(data: dict[str, Any]) -> None:
    global estop_active

    estop_active = bool(data.get("active", False))

    if estop_active:
        print("!!! EMERGENCY STOP !!!")
        clear_scheduled_actions()
        stop_all_vehicles()
        command_drop_track("hold", motor_a_speed=0, motor_b_speed=0)


def handle_mode(data: dict[str, Any]) -> None:
    global ride_mode

    ride_mode = data.get("mode", "manual")
    print("Ride mode:", ride_mode)

    if ride_mode != "auto":
        clear_scheduled_actions()


def handle_reset() -> None:
    global switch_waiting_for_alignment
    global switch_waiting_for_clear

    print("System reset")
    clear_scheduled_actions()
    stop_all_vehicles()
    command_switch_track(0)
    command_turntable(0)
    command_drop_track("top", motor_a_speed=180, motor_b_speed=180)
    switch_waiting_for_alignment = False
    switch_waiting_for_clear = False


def drive_vehicle(
    vehicle_id: str,
    speed: int,
    left_speed: int | None = None,
    right_speed: int | None = None,
) -> None:
    if left_speed is None or right_speed is None:
        pwm = max(-255, min(255, int(speed)))
        left_speed = pwm
        right_speed = pwm

    payload = {
        "speed": max(-255, min(255, int(speed))),
        "left_speed": left_speed,
        "right_speed": right_speed,
    }
    publish_json(f"ride/vehicle/{vehicle_id}/drive/command", payload)


def set_yaw(vehicle_id: str, angle: int) -> None:
    publish_json(
        f"ride/vehicle/{vehicle_id}/servoYaw/command",
        {"angle": angle},
    )


def command_switch_track(target_angle: int) -> None:
    payload = {"target_angle": target_angle}
    publish_json("ride/actuator/switchTrack/command", payload)
    # Also publish state so UI highlights
    publish_json("ride/actuator/switchTrack/state", {"angle": target_angle, "moving": False})


def command_turntable(target_angle: int) -> None:
    payload = {"target_angle": target_angle}
    publish_json("ride/actuator/rotateTrack/command", payload)
    # Also publish state so UI highlights
    publish_json("ride/actuator/rotateTrack/state", {"angle": target_angle, "moving": False})


def command_drop_track(target: str, motor_a_speed: int, motor_b_speed: int) -> None:
    payload = {
        "target": target,
        "motor_a_speed": motor_a_speed,
        "motor_b_speed": motor_b_speed,
    }
    publish_json("ride/actuator/dropTrack/command", payload)
    # Also publish state so UI highlights
    publish_json("ride/actuator/dropTrack/state", {"target": target, "moving": False})


def stop_all_vehicles() -> None:
    for vehicle_id in vehicles:
        drive_vehicle(vehicle_id, speed=0, left_speed=0, right_speed=0)


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

    print(f"Connecting to broker at {BROKER}:{PORT}...")
    try:
        client.connect(BROKER, PORT, keepalive=60)
    except Exception as e:
        print(f"Failed to connect: {e}")
        return

    client.loop_start()

    print("Ride controller running")

    last_heartbeat = 0.0

    while True:
        now = time.time()

        if now - last_heartbeat >= 1.0:
            send_heartbeat()
            last_heartbeat = now

        process_scheduled_actions()
        time.sleep(0.05)


if __name__ == "__main__":
    main()
