import paho.mqtt.client as mqtt
import json
import csv
import os
from datetime import datetime

# Konfigurasi broker
BROKER = "broker.emqx.io"
PORT = 1883
TOPIC = "waitress/+/+"  # Multi sender dan multi event (call/bill)

# Nama file CSV
CSV_FILE = "log.csv"

# Kolom untuk CSV
CSV_HEADER = ["timestamp", "device_id", "type", "status", "count", "rssi", "topic"]

# Buat file log jika belum ada
if not os.path.exists(CSV_FILE):
    with open(CSV_FILE, mode="w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(CSV_HEADER)

# Callback saat berhasil connect ke broker
def on_connect(client, userdata, flags, rc):
    print(f"Connected to broker with result code {rc}")
    client.subscribe(TOPIC)

# Callback saat pesan diterima
def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)

        # Ambil data
        row = [
            data.get("timestamp", datetime.utcnow().isoformat() + "Z"),
            data.get("id", ""),
            data.get("type", ""),
            data.get("status", ""),
            data.get("count", ""),
            data.get("rssi", ""),
            msg.topic
        ]

        # Tampilkan ke terminal
        print("Message received:", row)

        # Simpan ke CSV
        with open(CSV_FILE, mode="a", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(row)

    except Exception as e:
        print("Error processing message:", e)

# Inisialisasi MQTT Client
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)

# Looping terus
print("Listening for messages...")
client.loop_forever()
