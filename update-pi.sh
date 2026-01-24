#!/bin/bash
# Quick update script for Spånsug backend
# Only updates files and restarts services - no package installation

set -e

echo "=================================="
echo "Spånsug Backend - Quick Update"
echo "=================================="

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo "Please do not run as root (no sudo needed)"
   exit 1
fi

# Update web files
echo -e "\n${BLUE}[1/3] Updating web dashboard...${NC}"
if [ -f "dashboard.html" ]; then
    sudo cp dashboard.html /var/www/html/
    echo -e "${GREEN}✓ Updated dashboard.html${NC}"
fi

if [ -f "index.html" ]; then
    sudo cp index.html /var/www/html/
    echo -e "${GREEN}✓ Updated index.html${NC}"
fi

sudo systemctl restart apache2
echo -e "${GREEN}✓ Apache restarted${NC}"

# Update Node-RED flows (backup first)
echo -e "\n${BLUE}[2/3] Updating Node-RED flows...${NC}"
NODERED_DIR="/home/$USER/.node-red"

# Create backup
BACKUP_DIR="$NODERED_DIR/backup-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$BACKUP_DIR"
if [ -f "$NODERED_DIR/flows.json" ]; then
    cp "$NODERED_DIR/flows.json" "$BACKUP_DIR/"
    echo -e "${GREEN}✓ Backup created: $BACKUP_DIR${NC}"
fi

# Copy flow files
if [ -d "node-red" ]; then
    mkdir -p "$NODERED_DIR/flows-import"
    cp node-red/*.json "$NODERED_DIR/flows-import/"
    echo -e "${GREEN}✓ Flow files copied to $NODERED_DIR/flows-import/${NC}"
    echo -e "${BLUE}  Import manually in Node-RED UI at http://$(hostname -I | awk '{print $1}'):1880${NC}"
fi

# Restart Node-RED
echo -e "\n${BLUE}[3/3] Restarting Node-RED...${NC}"
sudo systemctl restart nodered
sleep 3

if sudo systemctl is-active --quiet nodered; then
    echo -e "${GREEN}✓ Node-RED restarted successfully${NC}"
else
    echo -e "\n${RED}✗ Node-RED failed to start${NC}"
    echo "Check logs: sudo journalctl -u nodered -n 50"
    exit 1
fi

# Summary
echo -e "\n${GREEN}=================================="
echo "Update Complete!"
echo "==================================${NC}"
echo ""
echo "Dashboard: http://$(hostname -I | awk '{print $1}')"
echo "Node-RED:  http://$(hostname -I | awk '{print $1}'):1880"
echo ""
echo "Next steps:"
echo "1. Import flows from ~/.node-red/flows-import/ in Node-RED UI"
echo "2. Click 'Deploy' in Node-RED"
echo ""
