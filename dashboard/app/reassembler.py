import json
import os
import requests
import paho.mqtt.client as mqtt
from datetime import datetime
import time

# Buffers for holding chunks
image_chunks = {}         # {image_id: {chunk_index: bytes}}
chunk_total = {}          # {image_id: total_chunks}
chunk_timestamp = {}      # {image_id: timestamp}

# Folder to save output
output_dir = "static/images"
os.makedirs(output_dir, exist_ok=True)

def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    client.subscribe("uts/iot13521111/imagechunk")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        img_id = payload["id"]
        ts = payload["ts"]
        chunk = payload["chunk"]
        total = payload["total"]
        hex_data = payload["data"]

        # Initialize structures
        if img_id not in image_chunks:
            image_chunks[img_id] = {}
            chunk_total[img_id] = total
            chunk_timestamp[img_id] = ts

        # Add chunk
        image_chunks[img_id][chunk] = bytes.fromhex(hex_data)
        print(f"Received chunk {chunk + 1}/{total} for image {img_id}")

        # Check if complete
        if len(image_chunks[img_id]) == total:
            print(f"Reassembling image {img_id}...")

            # Reconstruct image
            all_data = b''.join(image_chunks[img_id][i] for i in range(total))

            # Save it
            dt = datetime.fromtimestamp(chunk_timestamp[img_id])
            filename = f"image_{img_id}_{dt.strftime('%Y%m%d_%H%M%S')}.jpg"
            filepath = os.path.join(output_dir, filename)

            with open(filepath, "wb") as f:
                f.write(all_data)

            received_time = time.time()
            sent_time = chunk_timestamp[img_id]

            latency = received_time - sent_time
            print(f"Latency for image {img_id}: {latency:.3f} seconds")
            
            # Notify Flask dashboard
            try:
                requests.post("http://localhost:5000/notify", json={"filename": filename})
            except Exception as e:
                print("Notification failed:", e)

            # Cleanup
            del image_chunks[img_id]
            del chunk_total[img_id]
            del chunk_timestamp[img_id]

    except Exception as e:
        print("Error:", e)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883, 60)

print("Listening for image chunks...")
client.loop_forever()
