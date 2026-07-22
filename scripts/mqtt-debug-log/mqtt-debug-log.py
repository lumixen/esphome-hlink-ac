#!/usr/bin/env python3
import paho.mqtt.client as mqtt
from datetime import datetime, date
import signal
import sys
import os
import yaml

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "config.yaml")

with open(CONFIG_PATH) as f:
    cfg = yaml.safe_load(f)

BROKER = cfg["mqtt"]["broker"]
PORT = cfg["mqtt"]["port"]
USER = cfg["mqtt"]["username"]
PASS = cfg["mqtt"]["password"]
TOPIC = cfg["mqtt"]["topic"]
LOG_DIR = cfg.get("log_dir", ".")

def log_path():
    os.makedirs(LOG_DIR, exist_ok=True)
    return os.path.join(LOG_DIR, f"debug-{date.today().isoformat()}.log")

def on_message(client, userdata, msg):
    ts = datetime.now().isoformat(timespec="milliseconds")
    payload = msg.payload.decode(errors="replace")
    with open(log_path(), "a") as f:
        f.write(f"[{ts}] {payload}\n")
    print(f"[{ts}] {payload}", flush=True)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to {BROKER}, subscribing to '{TOPIC}'", flush=True)
        client.subscribe(TOPIC)
    else:
        print(f"Connection failed (RC {rc})", flush=True)

def on_disconnect(client, userdata, rc):
    print(f"Disconnected (RC {rc}), reconnecting...", flush=True)

def main():
    client = mqtt.Client()
    client.username_pw_set(USER, PASS)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    def shutdown(signum, frame):
        print("Shutting down...", flush=True)
        client.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    try:
        print(f"Connecting to {BROKER}:{PORT}...", flush=True)
        client.connect(BROKER, PORT, keepalive=60)
        client.loop_forever()
    except Exception as e:
        print(f"Error: {e}", flush=True)
        sys.exit(1)

if __name__ == "__main__":
    main()
