# Deploy script til Raspberry Pi backend
# Kører fra Windows til Raspberry Pi

param(
    [switch]$QuickUpdate = $false
)

$PI_HOST = "spansug-backend.local" # Raspberry Pi hostname
$PI_USER = "pi" # standard bruger på Raspberry Pi
$PI_PASS = "raspberry" # standard password på Raspberry Pi (ændr hvis nødvendigt)

if ($QuickUpdate) {
    Write-Host "=== Quick Update til Raspberry Pi ===" -ForegroundColor Cyan
} else {
    Write-Host "=== Deploy til Raspberry Pi Backend ===" -ForegroundColor Cyan
}
Write-Host "Host: $PI_HOST" -ForegroundColor Yellow

# Tjek om SSH key findes, ellers opret og kopier til Pi
$sshKeyPath = "$env:USERPROFILE\.ssh\id_rsa"
if (-not (Test-Path $sshKeyPath)) {
    Write-Host "`nIngen SSH key fundet. Opretter ny SSH key..." -ForegroundColor Yellow
    ssh-keygen -t rsa -b 4096 -f $sshKeyPath -N '""' -q
    Write-Host "SSH key oprettet!" -ForegroundColor Green
}

# Tjek om key allerede er kopieret til Pi
Write-Host "`nTjekker SSH key authentication..." -ForegroundColor Green
ssh -o BatchMode=yes -o ConnectTimeout=5 ${PI_USER}@${PI_HOST} "echo 'Key auth OK'" 2>$null

if ($LASTEXITCODE -ne 0) {
    Write-Host "SSH key ikke fundet på Pi. Kopierer key..." -ForegroundColor Yellow
    Write-Host "Du bliver bedt om password én gang for at sætte key-based auth op." -ForegroundColor Yellow
    
    # Kopier SSH key til Pi
    $pubKeyPath = "$sshKeyPath.pub"
    type $pubKeyPath | ssh ${PI_USER}@${PI_HOST} "mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "SSH key kopieret! Fremtidige forbindelser kræver ikke password." -ForegroundColor Green
    } else {
        Write-Host "FEJL: Kunne ikke kopiere SSH key" -ForegroundColor Red
        exit 1
    }
}

# Test SSH forbindelse
Write-Host "`nTester SSH forbindelse..." -ForegroundColor Green
ssh ${PI_USER}@${PI_HOST} "echo 'SSH forbindelse OK'"

if ($LASTEXITCODE -ne 0) {
    Write-Host "FEJL: Kan ikke forbinde til Raspberry Pi" -ForegroundColor Red
    Write-Host "Tjek at SSH er aktiveret og Pi'en er tilgængelig på netværket" -ForegroundColor Yellow
    exit 1
}

# Opret mapper på Pi
Write-Host "`nOpretter mapper på Raspberry Pi..." -ForegroundColor Green
ssh ${PI_USER}@${PI_HOST} "mkdir -p ~/spansug-backend/node-red && mkdir -p ~/spansug-backend/web"

# Kopier Node-RED flows
Write-Host "`nKopierer Node-RED flows..." -ForegroundColor Green
scp node-red/*.json ${PI_USER}@${PI_HOST}:~/spansug-backend/node-red/

# Kopier web dashboard
Write-Host "`nKopierer web dashboard..." -ForegroundColor Green
scp web/dashboard.html ${PI_USER}@${PI_HOST}:~/spansug-backend/web/

# Kopier index.html fra data mappen
Write-Host "`nKopierer data filer..." -ForegroundColor Green
scp data/index.html ${PI_USER}@${PI_HOST}:~/spansug-backend/web/

# Publicer web filer til Apache document root
Write-Host "`nPublicerer web filer til /var/www/html/..." -ForegroundColor Green
ssh ${PI_USER}@${PI_HOST} "sudo cp ~/spansug-backend/web/dashboard.html /var/www/html/dashboard.html && sudo cp ~/spansug-backend/web/index.html /var/www/html/index.html"

# Kopier debug script
Write-Host "`nKopierer debug script..." -ForegroundColor Green
scp debug-backend.sh ${PI_USER}@${PI_HOST}:~/spansug-backend/
ssh ${PI_USER}@${PI_HOST} "chmod +x ~/spansug-backend/debug-backend.sh"

# Kopier installations script
Write-Host "`nKopierer installations script..." -ForegroundColor Green
scp install-pi.sh ${PI_USER}@${PI_HOST}:~/spansug-backend/
ssh ${PI_USER}@${PI_HOST} "chmod +x ~/spansug-backend/install-pi.sh"

# Kopier Node-RED import script
Write-Host "`nKopierer Node-RED import script..." -ForegroundColor Green
scp import-nodered-flows.sh ${PI_USER}@${PI_HOST}:~/spansug-backend/
ssh ${PI_USER}@${PI_HOST} "chmod +x ~/spansug-backend/import-nodered-flows.sh"

# Hvis QuickUpdate, importer flows automatisk
if ($QuickUpdate) {
    Write-Host "`nImporterer Node-RED flows..." -ForegroundColor Green
    ssh ${PI_USER}@${PI_HOST} "cd ~/spansug-backend && bash import-nodered-flows.sh"
}

Write-Host "`n=== Deploy færdig! ===" -ForegroundColor Cyan

if ($QuickUpdate) {
    Write-Host "`nFlows er importeret og Node-RED kør nu!" -ForegroundColor Green
    Write-Host "Tjek Node-RED på: http://spansug-backend.local:1880" -ForegroundColor Green
} else {
    Write-Host "`nNæste skridt:" -ForegroundColor Yellow
    Write-Host "1. SSH til Pi'en: ssh pi@spansug-backend.local"
    Write-Host "2. Kør installations scriptet: cd ~/spansug-backend && bash install-pi.sh"
    Write-Host "3. Importer Node-RED flows: cd ~/spansug-backend && bash import-nodered-flows.sh"
}
