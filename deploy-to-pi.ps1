# Deploy script til Raspberry Pi backend
# Kører fra Windows til Raspberry Pi

$PI_IP = "192.168.87.110"
$PI_USER = "pi"
$PI_PASS = "raspberry"

Write-Host "=== Deploy til Raspberry Pi Backend ===" -ForegroundColor Cyan
Write-Host "IP: $PI_IP" -ForegroundColor Yellow

# Test SSH forbindelse
Write-Host "`nTester SSH forbindelse..." -ForegroundColor Green
ssh ${PI_USER}@${PI_IP} "echo 'SSH forbindelse OK'"

if ($LASTEXITCODE -ne 0) {
    Write-Host "FEJL: Kan ikke forbinde til Raspberry Pi" -ForegroundColor Red
    Write-Host "Tjek at SSH er aktiveret og Pi'en er tilgængelig på netværket" -ForegroundColor Yellow
    exit 1
}

# Opret mapper på Pi
Write-Host "`nOpretter mapper på Raspberry Pi..." -ForegroundColor Green
ssh ${PI_USER}@${PI_IP} "mkdir -p ~/spansug-backend/node-red && mkdir -p ~/spansug-backend/web"

# Kopier Node-RED flows
Write-Host "`nKopierer Node-RED flows..." -ForegroundColor Green
scp node-red/*.json ${PI_USER}@${PI_IP}:~/spansug-backend/node-red/

# Kopier web dashboard
Write-Host "`nKopierer web dashboard..." -ForegroundColor Green
scp web/dashboard.html ${PI_USER}@${PI_IP}:~/spansug-backend/web/

# Kopier index.html fra data mappen
Write-Host "`nKopierer data filer..." -ForegroundColor Green
scp data/index.html ${PI_USER}@${PI_IP}:~/spansug-backend/web/

# Kopier debug script
Write-Host "`nKopierer debug script..." -ForegroundColor Green
scp debug-backend.sh ${PI_USER}@${PI_IP}:~/spansug-backend/
ssh ${PI_USER}@${PI_IP} "chmod +x ~/spansug-backend/debug-backend.sh"

# Kopier installations script
Write-Host "`nKopierer installations script..." -ForegroundColor Green
scp install-pi.sh ${PI_USER}@${PI_IP}:~/spansug-backend/
ssh ${PI_USER}@${PI_IP} "chmod +x ~/spansug-backend/install-pi.sh"

Write-Host "`n=== Deploy færdig! ===" -ForegroundColor Cyan
Write-Host "`nNæste skridt:" -ForegroundColor Yellow
Write-Host "1. SSH til Pi'en: ssh pi@192.168.87.110"
Write-Host "2. Kør installations scriptet: cd ~/spansug-backend && bash install-pi.sh"
Write-Host "3. Importer Node-RED flows via web interface: http://192.168.87.110:1880"
