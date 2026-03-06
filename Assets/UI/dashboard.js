// Change based on computer using
const client = mqtt.connect("ws://localhost:9001");

const vehicle = document.getElementById("vehicle");

const positions = {

    Station1: [240, 445],
    Station2: [380, 445],
    Centry: [700, 445],
    Switch1: [780, 100],
    Switch2: [720, 90],
    Rotate1: [530, 90],
    Rotate2: [500, 110],
    Basket: [450, 190],
    Mid: [180, 50],
    Drop1: [70, 140],
    Drop2: [65, 370]

};

client.on("connect", () => {

    console.log("Connected to MQTT");

    // Subscribe to all sensor state messages
    client.subscribe("ride/sensor/+/state");

});

client.on("message", (topic, message) => {

    const raw = message.toString();

    console.log("MQTT message:", topic, raw);

    let data;

    try {
        data = JSON.parse(raw);
    } catch (e) {
        console.log("Bad JSON:", raw);
        return;
    }

    const state = data.state;

    // Extract sensor name from topic
    const parts = topic.split("/");
    const sensorId = parts[2];

    if(state === 1){
        moveVehicle(sensorId);
    }

});

function moveVehicle(zone){

    if(!(zone in positions)){
        console.log("Unknown zone:", zone);
        return;
    }

    const pos = positions[zone];

    vehicle.setAttribute("cx", pos[0]);
    vehicle.setAttribute("cy", pos[1]);

    console.log("Vehicle moved to:", zone);

}