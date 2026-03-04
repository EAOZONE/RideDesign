import tkinter as tk
from PIL import Image, ImageTk
import paho.mqtt.client as mqtt

BROKER = "10.160.121.73"
PORT = 1883

# Track section colors
ACTIVE = "red"
INACTIVE = ""

root = tk.Tk()
root.title("Ride Track Monitor")

canvas = tk.Canvas(root, width=900, height=600)
canvas.pack()

# Load track map
img = Image.open("track_map.jpg")
img = img.resize((900,600))
tk_img = ImageTk.PhotoImage(img)

canvas.create_image(0,0,anchor="nw",image=tk_img)

# Define zones (x1,y1,x2,y2)
zones = {
    "Midtown": canvas.create_rectangle(200,80,350,200,outline="",fill=""),
    "Lake": canvas.create_rectangle(350,200,520,380,outline="",fill=""),
    "Baseball": canvas.create_rectangle(550,180,700,300,outline="",fill=""),
    "Library": canvas.create_rectangle(720,50,850,150,outline="",fill=""),
    "CT": canvas.create_rectangle(700,380,850,520,outline="",fill=""),
    "Station": canvas.create_rectangle(300,500,500,580,outline="",fill="")
}

current_zone = None


def on_connect(client, userdata, flags, rc):
    print("Connected")
    client.subscribe("ride/sensor/#")


def on_message(client, userdata, msg):
    global current_zone

    zone = msg.topic.split("/")[-1]

    if zone in zones:

        # turn previous zone off
        if current_zone:
            canvas.itemconfig(zones[current_zone], fill=INACTIVE)

        # light up new zone
        canvas.itemconfig(zones[zone], fill=ACTIVE)

        current_zone = zone


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT)
client.loop_start()

root.mainloop()