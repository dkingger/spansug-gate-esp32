#!/bin/bash
# Import all Node-RED flows by directly updating flows.json
# Uses Python to properly merge JSON arrays
# Usage: bash import-nodered-flows.sh

NODERED_DIR="$HOME/.node-red"
PROJECT_DIR="$NODERED_DIR/projects/Spaansug"
FLOWS_JSON="$PROJECT_DIR/flows.json"
if [ ! -f "$PROJECT_DIR/package.json" ] || [ ! -f "$FLOWS_JSON" ]; then
    echo "ERROR: Expected existing Spaansug project not found: $PROJECT_DIR"
    exit 1
fi
PENDING_JSON=$(mktemp "$PROJECT_DIR/.flows-import.XXXXXX")
trap 'rm -f "$PENDING_JSON"' EXIT
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FLOWS_DIR="$SCRIPT_DIR/../node-red"
# Deployment copies scripts beside node-red on the Pi.
if [ -d "$SCRIPT_DIR/node-red" ]; then FLOWS_DIR="$SCRIPT_DIR/node-red"; fi
BACKUP_DIR="$NODERED_DIR/backups"

echo "=================================="
echo "Node-RED Flow Importer"
echo "=================================="
echo "Flows directory: $FLOWS_DIR"
echo "Node-RED home: $NODERED_DIR"
echo ""

# Check if flows directory exists
if [ ! -d "$FLOWS_DIR" ]; then
    echo "❌ ERROR: Flows directory not found: $FLOWS_DIR"
    exit 1
fi

# Check flow files exist
FLOW_COUNT=$(ls -1 "$FLOWS_DIR"/spansug-*.json 2>/dev/null | wc -l)
if [ "$FLOW_COUNT" -eq 0 ]; then
    echo "❌ ERROR: No flows found in $FLOWS_DIR"
    exit 1
fi

echo "Found $FLOW_COUNT flow files to import"
echo ""

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Backup current flows.json
if [ -f "$FLOWS_JSON" ]; then
    BACKUP_FILE="$BACKUP_DIR/flows-backup-$(date +%Y%m%d-%H%M%S).json"
    cp "$FLOWS_JSON" "$BACKUP_FILE"
    echo "✓ Backed up current flows.json to: $BACKUP_FILE"
else
    echo "ℹ️  No existing flows.json found - creating new one"
fi

# Use Python to merge all JSON files properly
export FLOWS_DIR="$FLOWS_DIR"
export FLOWS_JSON="$PENDING_JSON"
python3 << 'PYTHON_SCRIPT'
import json
import os
import glob
from pathlib import Path

flows_dir = os.environ['FLOWS_DIR']
flows_json = os.environ['FLOWS_JSON']
all_nodes = []

# Read and merge all flow files
flow_files = sorted(glob.glob(os.path.join(flows_dir, 'spansug-*.json')))
print(f"Processing {len(flow_files)} flow files...")

for flow_file in flow_files:
    try:
        with open(flow_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            if isinstance(data, list):
                all_nodes.extend(data)
                print(f"  ✓ {os.path.basename(flow_file)}: {len(data)} nodes")
            elif isinstance(data, dict):
                all_nodes.append(data)
                print(f"  ✓ {os.path.basename(flow_file)}: 1 node")
    except Exception as e:
        print(f"  ❌ Error reading {os.path.basename(flow_file)}: {e}")
        exit(1)

# Write combined flows
with open(flows_json, 'w', encoding='utf-8') as f:
    json.dump(all_nodes, f, indent=2)

print(f"\n✓ Combined {len(all_nodes)} total nodes into flows.json")
PYTHON_SCRIPT

# Check if Python script succeeded
if [ $? -ne 0 ]; then
    echo "❌ ERROR: Failed to merge flows"
    exit 1
fi

echo ""
echo "Stopping Node-RED..."
sudo systemctl stop nodered || exit 1
sleep 2

if ! cp "$PENDING_JSON" "$FLOWS_JSON"; then
    sudo systemctl start nodered
    exit 1
fi

echo "Starting Node-RED..."
sudo systemctl start nodered
sleep 4

# Check if Node-RED started
if sudo systemctl is-active --quiet nodered; then
    echo "✓ Node-RED started successfully"
    echo ""
    echo "🎉 All flows imported and Node-RED is running!"
    echo ""
    PI_IP=$(hostname -I | awk '{print $1}')
    echo "Access Node-RED at: http://$PI_IP:1880"
else
    echo "❌ ERROR: Node-RED failed to start"
    echo "Check logs: sudo journalctl -u nodered -n 20"
    exit 1
fi
