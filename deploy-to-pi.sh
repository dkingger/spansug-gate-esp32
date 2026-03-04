#!/bin/bash
# Deploy script til Raspberry Pi backend
# Kører fra Mac/Linux til Raspberry Pi

PI_HOST="spansug-backend.local"
PI_USER="pi"

echo "=== Deploy til Raspberry Pi Backend ==="
echo "Host: $PI_HOST"

# Tjek om SSH key findes
if [ ! -f ~/.ssh/id_rsa ]; then
    echo -e "\nIngen SSH key fundet. Opretter ny SSH key..."
    ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa -N "" -q
    echo "SSH key oprettet!"
fi

# Tjek om key allerede er kopieret til Pi
echo -e "\nTjekker SSH key authentication..."
if ! ssh -o BatchMode=yes -o ConnectTimeout=5 ${PI_USER}@${PI_HOST} "echo 'Key auth OK'" 2>/dev/null; then
    echo "SSH key ikke fundet på Pi. Kopierer key..."
    echo "Du bliver bedt om password én gang for at sætte key-based auth op."
    
    # Kopier SSH key til Pi
    ssh-copy-id ${PI_USER}@${PI_HOST}
    
    if [ $? -eq 0 ]; then
        echo "SSH key kopieret! Fremtidige forbindelser kræver ikke password."
    else
        echo "FEJL: Kunne ikke kopiere SSH key"
        exit 1
    fi
fi

# Test SSH forbindelse
echo -e "\nTester SSH forbindelse..."
if ! ssh ${PI_USER}@${PI_HOST} "echo 'SSH forbindelse OK'"; then
    echo "FEJL: Kan ikke forbinde til Raspberry Pi"
    echo "Tjek at SSH er aktiveret og Pi'en er tilgængelig på netværket"
    exit 1
fi

# Opret mapper på Pi
echo -e "\nOpretter mapper på Raspberry Pi..."
ssh ${PI_USER}@${PI_HOST} "mkdir -p ~/spansug-backend/node-red && mkdir -p ~/spansug-backend/web"

# Kopier Node-RED flows
echo -e "\nKopierer Node-RED flows..."
scp node-red/*.json ${PI_USER}@${PI_HOST}:~/spansug-backend/node-red/

# Kopier web dashboard
echo -e "\nKopierer web dashboard..."
scp web/dashboard.html ${PI_USER}@${PI_HOST}:~/spansug-backend/web/

# Kopier index.html fra data mappen
echo -e "\nKopierer data filer..."
scp data/index.html ${PI_USER}@${PI_HOST}:~/spansug-backend/web/

# Publicer web filer til Apache document root
echo -e "\nPublicerer web filer til /var/www/html/..."
ssh ${PI_USER}@${PI_HOST} "sudo cp ~/spansug-backend/web/dashboard.html /var/www/html/dashboard.html && sudo cp ~/spansug-backend/web/index.html /var/www/html/index.html"

# Kopier debug script
echo -e "\nKopierer debug script..."
scp debug-backend.sh ${PI_USER}@${PI_HOST}:~/spansug-backend/
ssh ${PI_USER}@${PI_HOST} "chmod +x ~/spansug-backend/debug-backend.sh"

# Kopier installations script
echo -e "\nKopierer installations script..."
scp install-pi.sh ${PI_USER}@${PI_HOST}:~/spansug-backend/
ssh ${PI_USER}@${PI_HOST} "chmod +x ~/spansug-backend/install-pi.sh"

echo -e "\n=== Deploy færdig! ==="
echo -e "\nNæste skridt:"
echo "1. SSH til Pi'en: ssh pi@spansug-backend.local"
echo "2. Kør installations scriptet: cd ~/spansug-backend && bash install-pi.sh"
echo "3. Importer Node-RED flows via web interface: http://spansug-backend.local:1880"
