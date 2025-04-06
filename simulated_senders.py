import paho.mqtt.client as mqtt
import json
import time
import random
import threading
from datetime import datetime

# MQTT Broker
BROKER = "broker.emqx.io"
PORT = 1883

# Daftar device ID
SENDERS = ["Sender1", "Sender2", "Sender3"]

# Fungsi untuk publish data dari satu sender
def sender_loop(sender_id):
    client = mqtt.Client()
    client.connect(BROKER, PORT, 60)
    client.loop_start()

    call_count = 0
    bill_count = 0

    while True:
        action = random.choice(["call", "bill"])
        rssi = random.randint(-80, -40)
        timestamp = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

        if action == "call":
            call_count += 1
            count = call_count
        else:
            bill_count += 1
            count = bill_count

        payload = {
            "id": sender_id,
            "type": action,
            "status": True,
            "count": count,
            "rssi": rssi,
            "timestamp": timestamp
        }

        topic = f"waitress/{sender_id}/{action}"
        client.publish(topic, json.dumps(payload), qos=1, retain=True)
        print(f"[{sender_id}] Published to {topic}: {payload}")

        time.sleep(random.uniform(2, 4))  # Random delay antar kiriman

# Jalankan setiap sender di thread terpisah
threads = []
for sender_id in SENDERS:
    t = threading.Thread(target=sender_loop, args=(sender_id,))
    t.start()
    threads.append(t)

# Keep main thread running
for t in threads:
    t.join()
