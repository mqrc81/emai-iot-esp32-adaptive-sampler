import json
import random


def load_mqtt(path: str) -> list[dict]:
    random.seed(45)
    records = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(" ", 2)
            if len(parts) < 3:
                continue
            try:
                payload = json.loads(parts[2])
                payload["_mac_ts_us"] = int(parts[0])
                # payload["_mac_ts_us"] = int(parts[0].replace("N", "90000"))
                records.append(payload)
            except (json.JSONDecodeError, ValueError):
                continue
    return records
