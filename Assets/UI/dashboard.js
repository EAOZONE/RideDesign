// Change based on computer using
const client = mqtt.connect("ws://10.160.121.73:9001");

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

    client.subscribe("wayside/beam");

});

client.on("message", (topic, message) => {

    const raw = message.toString();
    console.log("MQTT message:", raw);

    let id = null;
    let state = 0;

    try {
        // Try normal JSON first
        const data = JSON.parse(raw);
        id = data.id;
        state = data.state;
    }
    catch(e){
        // Fallback parser for {id:SwitchTrack,state:1}
        const match = raw.match(/id:([^,}]+).*state:(\d+)/);
        if(match){
            id = match[1];
            state = parseInt(match[2]);
        }
    }

    if(state === 1){
        moveVehicle(id);
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