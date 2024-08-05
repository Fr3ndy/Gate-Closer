const char* html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="it">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Controllo Cancello</title>
        <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.2.0/dist/css/bootstrap.min.css" rel="stylesheet">
        <style>
            body { font-family: Arial, sans-serif; }
            #loading {
                display: none;
                position: fixed;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                background-color: rgba(0, 0, 0, 0.5);
                z-index: 9999;
                color: white;
                font-size: 30px;
                text-align: center;
                padding-top: 20%;
            }
            button:disabled { background-color: #292929 !important; }
        </style>
    </head>
    <body>
        <div id="loading">Caricamento...</div>
        <div class="container mt-5">
            <h1 class="text-center">Controllo Cancello</h1>
            <h2 class="text-center">Cancello Automatico</h2>
            <ul class="nav nav-tabs" id="myTab" role="tablist">
                <li class="nav-item" role="presentation">
                    <button class="nav-link active" id="generale-tab" data-bs-toggle="tab" data-bs-target="#generale" type="button" role="tab" aria-controls="generale" aria-selected="true">Generale</button>
                </li>
                <li class="nav-item" role="presentation">
                    <button class="nav-link" id="impostazioni-tab" data-bs-toggle="tab" data-bs-target="#impostazioni" type="button" role="tab" aria-controls="impostazioni" aria-selected="false">Impostazioni</button>
                </li>
            </ul>
            <div class="tab-content" id="myTabContent">
                <div class="tab-pane fade show active" id="generale" role="tabpanel" aria-labelledby="generale-tab">
                    <div class="section mt-4">
                        <h3>Generale</h3>
                        <button class="btn btn-lg btn-success" onclick="sendCommand('open')" id="openBtn">Apri</button>
                        <button class="btn btn-lg btn-warning" onclick="sendCommand('stop')">Stop</button>
                        <button class="btn btn-lg btn-primary" onclick="sendCommand('stayopen')">Lascia Aperto</button>
                    </div>
                    <div class="section mt-4">
                        <h3>Stato</h3>
                        <div id="status">-</div>
                    </div>
                    <div class="section mt-4">
                        <h3>Luce</h3>
                        <div id="light-status">-</div>
                    </div>
                </div>
                <div class="tab-pane fade" id="impostazioni" role="tabpanel" aria-labelledby="impostazioni-tab">
                    <div class="section mt-4">
                        <h3>Pulsantiera</h3>
                        <div class="form-group">
                            <label for="password">Password Apertura:</label>
                            <input type="text" class="form-control" id="password">
                        </div>
                        <div class="form-group">
                            <label for="behavior">Comportamento:</label>
                            <select class="form-control" id="behavior">
                                <option value="">Nessuno</option>
                                <option value="open">Apri</option>
                                <option value="stayopen">Lascia aperto</option>
                            </select>
                        </div>
                    </div>
                    <div class="section mt-4 d-flex justify-content-center flex-column">
                        <h3>Luce Cortesia</h3>
                        <div class="form-check">
                            <input type="checkbox" class="form-check-input" id="enableL">
                            <label class="form-check-label" for="enableL">Abilitato:</label>
                        </div>
                        <div class="form-group">
                            <label for="lightDuration">Tempo di attivazione (secondi):</label>
                            <input type="number" class="form-control" id="lightDuration" min="1">
                        </div>
                        <div class="form-group">
                            <label for="lightOnTime">Orario di attivazione:</label>
                            <input type="time" class="form-control" id="lightOnTime">
                        </div>
                        <div class="form-group">
                            <label for="lightOffTime">Orario di disattivazione:</label>
                            <input type="time" class="form-control" id="lightOffTime">
                        </div>
                        <button class="btn btn-primary mt-4" onclick="updateSettings()">Salva Impostazioni</button>
                    </div>
                </div>
            </div>
        </div>
    
        <!-- Modal -->
        <div class="modal fade" id="alertModal" tabindex="-1" aria-labelledby="alertModalLabel" aria-hidden="true">
          <div class="modal-dialog">
            <div class="modal-content">
              <div class="modal-header">
                <h5 class="modal-title" id="alertModalLabel">Avviso</h5>
                <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Chiudi"></button>
              </div>
              <div class="modal-body" id="alertModalBody">
              </div>
              <div class="modal-footer">
                <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Chiudi</button>
              </div>
            </div>
          </div>
        </div>
    
        <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.2.0/dist/js/bootstrap.bundle.min.js"></script>
        <script>
            function showLoading() {
                document.getElementById('loading').style.display = 'block';
            }
            
            function hideLoading() {
                document.getElementById('loading').style.display = 'none';
            }
            
            function showAlert(message) {
                document.getElementById('alertModalBody').textContent = message;
                new bootstrap.Modal(document.getElementById('alertModal')).show();
            }
            
            function handleResponseCode(code) {
                switch (code) {
                    case 1:
                        showAlert('Operazione di apertura completata con successo');
                        break;
                    case 2:
                        showAlert('Operazione di stop completata con successo');
                        break;
                    case 3:
                        showAlert('Cancello in movimento');
                        break;
                    case 4:
                        showAlert('Operazione di apertura fissa completata con successo');
                        break;
                    case 5:
                        showAlert('Errore di deserializzazione durante l\'impostazione');
                        break;
                    case 6:
                        showAlert('Impostazioni salvate');
                        break;
                    case 7:
                        showAlert('Nessun dato inviato per l\'impostazione');
                        break;
                    case 8:
                        showAlert('Impostazioni restituite con successo');
                        break;
                    case 9:
                        showAlert('Stato del cancello restituito con successo');
                        break;
                    default:
                        showAlert('Operazione non riuscita');
                        break;
                }
            }
            
            function sendCommand(cmd) {
                showLoading();
                fetch('/' + cmd)
                    .then(response => response.json())
                    .then(data => {
                        handleResponseCode(data.code);
                        updateStatus();
                    })
                    .finally(hideLoading);
            }
            
            function updateSettings() {
                showLoading();
                const settings = {
                    enableL: document.getElementById('enableL').checked,
                    lightOnTime: timeToMinutes(document.getElementById('lightOnTime').value),
                    lightOffTime: timeToMinutes(document.getElementById('lightOffTime').value),
                    lightDuration: document.getElementById('lightDuration').value,
                    password: document.getElementById('password').value,
                    behavior: document.getElementById('behavior').value
                };
                fetch('/setsettings', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(settings)
                })
                    .then(response => response.json())
                    .then(data => {
                        handleResponseCode(data.code);
                    })
                    .finally(hideLoading);
            }
            
            function timeToMinutes(time) {
                const [hours, minutes] = time.split(':');
                return parseInt(hours) * 60 + parseInt(minutes);
            }
            
            function minutesToTime(minutes) {
                const hours = Math.floor(minutes / 60);
                const mins = minutes % 60;
                return `${hours.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}`;
            }
            
            function updateStatus() {
                fetch('/getstatus')
                    .then(response => response.json())
                    .then(data => {
                        if (data.code === 9) {
                            let gateStatusText;
                            let gateStatusCode = data.gateStatus;
            
                            if (gateStatusCode === 1) {
                                gateStatusText = 'Aperto fisso';
                            } else if (gateStatusCode === 2) {
                                gateStatusText = 'Chiuso';
                            } else if (gateStatusCode === 3) {
                                gateStatusText = 'In movimento';
                            }
                            document.getElementById('status').textContent = 'Stato cancello: ' + gateStatusText;
            
                            let lightStatusText = (data.lightStatus === 1) ? 'Accesa' : 'Spenta';
                            document.getElementById('light-status').textContent = 'Stato luce: ' + lightStatusText;
            
                            if (gateStatusCode === 3) {
                                document.getElementById('openBtn').disabled = true;
                                document.getElementById('openBtn').innerHTML = "-";
                            } else if (gateStatusCode === 1) {
                                document.getElementById('openBtn').disabled = false;
                                document.getElementById('openBtn').innerHTML = "Chiudi";
                            } else {
                                document.getElementById('openBtn').disabled = false;
                                document.getElementById('openBtn').innerHTML = "Apri";
                            }
                        } else {
                            handleResponseCode(data.code);
                        }
                    });
            }
            
            fetch('/getsettings')
                .then(response => response.json())
                .then(data => {
                    if (data.code === 8) {
                        document.getElementById('enableL').checked = data.settings.enableL;
                        document.getElementById('lightOnTime').value = minutesToTime(data.settings.lightOnTime);
                        document.getElementById('lightOffTime').value = minutesToTime(data.settings.lightOffTime);
                        document.getElementById('password').value = data.settings.password;
                        document.getElementById('lightDuration').value = data.settings.lightDuration;
                        document.getElementById('behavior').value = data.settings.behavior;
                        handleResponseCode(data.code);
                    } else {
                        handleResponseCode(data.code);
                    }
                });
            
            updateStatus();
            
            setInterval(updateStatus, 1000);  // Aggiorna lo stato ogni secondo
        </script>
    </body>
    </html>
)rawliteral";