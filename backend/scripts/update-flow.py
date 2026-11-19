#!/usr/bin/env python3
import json

# Load existing flows
with open('/home/pi/.node-red/flows.json', 'r') as f:
    flows = json.load(f)

# Load new flow
with open('/tmp/spansug-gate-status-leds.json', 'r') as f:
    new_flow = json.load(f)

# Remove old gate_status_leds_flow nodes
flows = [f for f in flows if not (f.get('type') == 'tab' and f.get('id') == 'gate_status_leds_flow')]
flows = [f for f in flows if f.get('z') != 'gate_status_leds_flow']

# Add new flow
flows.extend(new_flow)

# Save updated flows
with open('/home/pi/.node-red/flows.json', 'w') as f:
    json.dump(flows, f, indent=2)

print("Flow updated successfully")
