#!/bin/bash
# Script til at diagnosticere backend problemer

echo "=== Tjekker Node-RED status ==="
sudo systemctl status nodered

echo -e "\n=== Tjekker Apache/Nginx status ==="
sudo systemctl status apache2 || sudo systemctl status nginx

echo -e "\n=== Tjekker Mosquitto status ==="
sudo systemctl status mosquitto

echo -e "\n=== Tjekker om Node-RED lytter på port 1880 ==="
sudo netstat -tlnp | grep 1880 || sudo ss -tlnp | grep 1880

echo -e "\n=== Tjekker om web server filer findes ==="
ls -la /var/www/html/dashboard.html

echo -e "\n=== Tjekker firewall status ==="
sudo ufw status

echo -e "\n=== Tjekker Node-RED konfiguration ==="
grep "uiHost" ~/.node-red/settings.js || echo "Ingen uiHost begrænsning fundet"
