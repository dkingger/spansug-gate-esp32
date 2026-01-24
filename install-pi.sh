#!/bin/bash
# Installation script til Raspberry Pi backend
# Kør dette script på Raspberry Pi'en

set -e

echo "=== Spånsug Backend Installation ==="
echo "Dette script installerer alle nødvendige komponenter"
echo ""

# Farver til output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Opdater system
echo -e "${GREEN}[1/7] Opdaterer system...${NC}"
sudo apt update && sudo apt upgrade -y

# Installer Mosquitto (MQTT Broker)
echo -e "${GREEN}[2/7] Installerer Mosquitto MQTT broker...${NC}"
sudo apt install -y mosquitto mosquitto-clients

# Konfigurer Mosquitto til at lytte på alle interfaces
echo -e "${GREEN}Konfigurerer Mosquitto...${NC}"
sudo tee /etc/mosquitto/conf.d/local.conf > /dev/null <<EOF
listener 1883
allow_anonymous true

listener 9001
protocol websockets
EOF

sudo systemctl enable mosquitto
sudo systemctl restart mosquitto

# Test Mosquitto
echo -e "${YELLOW}Test: Mosquitto status${NC}"
sudo systemctl status mosquitto --no-pager | head -5

# Installer Node-RED
echo -e "${GREEN}[3/7] Installerer Node-RED...${NC}"
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered) --confirm-install --confirm-pi

# Start Node-RED
sudo systemctl enable nodered.service
sudo systemctl start nodered.service

echo -e "${YELLOW}Venter på at Node-RED starter op...${NC}"
sleep 10

# Installer nødvendige Node-RED nodes
echo -e "${GREEN}[4/7] Installerer Node-RED nodes...${NC}"
cd ~/.node-red
npm install node-red-dashboard node-red-contrib-gpio

# Installer Apache web server
echo -e "${GREEN}[5/7] Installerer Apache web server...${NC}"
sudo apt install -y apache2

# Kopier web filer
echo -e "${GREEN}[6/7] Kopierer web filer...${NC}"
if [ -f ~/spansug-backend/web/dashboard.html ]; then
    sudo cp ~/spansug-backend/web/dashboard.html /var/www/html/
    echo "Dashboard kopieret til /var/www/html/"
fi

if [ -f ~/spansug-backend/web/index.html ]; then
    sudo cp ~/spansug-backend/web/index.html /var/www/html/
    echo "Index kopieret til /var/www/html/"
fi

sudo systemctl enable apache2
sudo systemctl restart apache2

# Konfigurer hostname
echo -e "${GREEN}[7/7] Konfigurerer hostname...${NC}"
sudo hostnamectl set-hostname spansug-backend

# Installer avahi for .local DNS
sudo apt install -y avahi-daemon
sudo systemctl enable avahi-daemon
sudo systemctl start avahi-daemon

# Opsummering
echo ""
echo -e "${GREEN}=== Installation færdig! ===${NC}"
echo ""
echo -e "${YELLOW}Services status:${NC}"
echo "- Mosquitto MQTT: $(systemctl is-active mosquitto)"
echo "- Node-RED: $(systemctl is-active nodered)"
echo "- Apache: $(systemctl is-active apache2)"
echo ""
echo -e "${YELLOW}Adgang til services:${NC}"
echo "- Node-RED editor: http://192.168.87.110:1880"
echo "- Dashboard: http://192.168.87.110/dashboard.html"
echo "- MQTT broker: 192.168.87.110:1883 (ESP32)"
echo "- MQTT WebSocket: 192.168.87.110:9001 (Dashboard)"
echo "- Hostname: spansug-backend.local"
echo ""
echo -e "${YELLOW}Næste skridt:${NC}"
echo "1. Åbn Node-RED editor: http://192.168.87.110:1880"
echo "2. Klik på hamburger menu (øverst højre) → Import"
echo "3. Importer alle .json filer fra ~/spansug-backend/node-red/"
echo "4. Klik 'Deploy' i Node-RED"
echo ""
echo "Test med: ~/spansug-backend/debug-backend.sh"
