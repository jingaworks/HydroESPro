let gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let currentEpoch = 0;
let myChart = null;

// window.addEventListener('load', () => {
//     initWebSocket();
//     loadLogsManager();
// });

// 1. WebSocket Engine
function initWebSocket() {
    websocket = new WebSocket(gateway);
    // Caută în script.js funcția initWebSocket și modifică blocul onopen:
    websocket.onopen = () => {
        document.getElementById('status-badge').className = "badge online";
        document.getElementById('status-badge').innerText = "Conectat";

        // NOU: Sincronizare automată a timpului din browser către ESP32 la conectare
        // Trimitem timestamp-ul Unix în secunde (Date.now() întoarce milisecunde)
        const browserUnixTime = Math.floor(Date.now() / 1000);

        const timeSyncPayload = {
            id: "sync_browser_time",
            value: browserUnixTime.toString()
        };

        // Trimitem comanda prin WebSocket
        websocket.send(JSON.stringify(timeSyncPayload));
        console.log("Sincronizare automată: S-a trimis timpul browserului către ESP32:", browserUnixTime);
    };

    websocket.onclose = () => {
        document.getElementById('status-badge').className = "badge offline";
        document.getElementById('status-badge').innerText = "Deconectat";
        setTimeout(initWebSocket, 2000);
    };
    websocket.onmessage = onMessage;
}

// 2. Ceas Local (rulează secundă de secundă în browser)
setInterval(() => {
    if (currentEpoch > 0) {
        currentEpoch++;
        const date = new Date(currentEpoch * 1000);
        const timeStr = [date.getHours(), date.getMinutes(), date.getSeconds()].map(v => String(v).padStart(2, '0')).join(':');
        const timeEl = document.getElementById('val-time');
        if (timeEl) timeEl.innerText = timeStr;
    }
}, 1000);

// 3. Trimitere modificări setări prin WebSocket (JSON clar)
document.querySelectorAll('.action').forEach(element => {
    element.addEventListener('change', (event) => {
        if (websocket.readyState === WebSocket.OPEN) {
            const data = { id: event.target.id, value: event.target.value };
            websocket.send(JSON.stringify(data));
        }
    });
});

// 4. Procesare date de la ESP32 (Telemetrie la 5s sau pachet de "init")
function onMessage(event) {
    try {
        const myObj = JSON.parse(event.data);

        if (myObj.epoch !== undefined) currentEpoch = myObj.epoch;
        if (myObj.temperature !== undefined) document.getElementById('val-temp').innerText = myObj.temperature + " °C";
        if (myObj.humidity !== undefined) document.getElementById('val-hum').innerText = myObj.humidity + " %";
        if (myObj.ds_temp !== undefined) document.getElementById('val-ds-temp').innerText = myObj.ds_temp + " °C";

        // Stare pompe dinamice
        ['pompa_apa', 'pompa_aer'].forEach(id => {
            if (myObj[id] !== undefined) {
                const textEl = document.getElementById(`val-${id.replace('_', '-')}`);
                const cardEl = document.getElementById(`card-${id.replace('_', '-')}`);
                if (textEl && cardEl) {
                    textEl.innerText = myObj[id] === 1 ? "PORNITĂ" : "OPRITĂ";
                    myObj[id] === 1 ? cardEl.classList.add('pump-active') : cardEl.classList.remove('pump-active');
                }
            }
        });

        // Monitorizare Alarme în Dashboard
        const alarmSection = document.getElementById('alarm-section');
        const alarmMsg = document.getElementById('val-alarm-msg');
        if (alarmSection && alarmMsg) {
            let errors = [];
            if (myObj.tank_low === 1) errors.push("Nivel Apă Scăzut în Bazin! 💧");
            if (myObj.ds_temp !== undefined && myObj.ds_temp !== "--.-") {
                const tActual = parseFloat(myObj.ds_temp);
                const tMin = parseFloat(document.getElementById('alarm_t_min')?.value || 18);
                const shadowMax = parseFloat(document.getElementById('alarm_t_max')?.value || 26);
                if (tActual < tMin) errors.push(`Temperatură Apă Prea Mică (${tActual}°C)! ❄️`);
                if (tActual > shadowMax) errors.push(`Temperatură Apă Prea Mare (${tActual}°C)! 🔥`);
            }
            if (errors.length > 0) { alarmMsg.innerHTML = errors.join("<br>"); alarmSection.style.display = "block"; }
            else { alarmSection.style.display = "none"; }
        }

        // Diagnoze
        if (myObj.wifi_rssi !== undefined) document.getElementById('val-rssi').innerText = myObj.wifi_rssi;
        if (myObj.free_heap !== undefined) document.getElementById('val-heap').innerText = myObj.free_heap;
        if (myObj.up_time !== undefined) document.getElementById('val-uptime').innerText = myObj.up_time;

        // Auto-populare elemente la deschidere (Pachetul INIT)
        Object.keys(myObj).forEach(key => {
            const el = document.getElementById(key);
            if (el) el.value = myObj[key];
        });
    } catch (e) { console.log("Mesaj primit non-JSON:", event.data); }
}

// Adaugă la finalul fișierului script.js pentru integrarea Google Charts

// ============================================================================
// LOGICĂ AVANSATĂ GOOGLE CHARTS: OVERLAY SEPARAT ȘI NAVIGARE PREV/NEXT
// ============================================================================
// Structuri de memorie pentru navigare automată
if (typeof google !== 'undefined') {
    google.charts.load('current', { 'packages': ['corechart'] });
}

// Structuri de memorie globale pentru navigare și auto-resize
let allDhtFiles = [];
let allDsFiles = [];
let currentActiveFile = "";

let lastChartDataTable = null;
let activeChartTitle = "Today Readings";
let activeChartOptions = null;

window.addEventListener('load', () => {
    initWebSocket();
    loadLogsManager();
    setupOverlayEvents();
    setupArrowNavigationEvents();
});

// Forțează redesenarea graficului la dimensiunile exacte ale ferestrei curente
window.addEventListener('resize', () => {
    if (lastChartDataTable && typeof google !== 'undefined' && google.visualization) {
        const container = document.querySelector('#curve_chart');
        if (container && container.clientWidth > 0) {
            const chart = new google.visualization.LineChart(container);
            chart.draw(lastChartDataTable, activeChartOptions);
        }
    }
});

// Funcție custom pentru sortarea fișierelor după data din nume (De la cel mai NOU la cel mai VECHI)
function sortFilesByDateDescending(fileArray) {
    return fileArray.sort((a, b) => {
        // Extragem doar numerele din string (ex: "reset_31_10_2024.txt" -> ["31", "10", "2024"])
        const matchA = a.match(/\d+/g);
        const matchB = b.match(/\d+/g);

        // Dacă fișierul nu conține cifre (caz de siguranță), îl punem la final
        if (!matchA) return 1;
        if (!matchB) return -1;

        // Mapăm componentele în funcție de structura numelui:
        // Structură tipică: dht_DD_MM_YYYY.txt sau reset_DD_MM_YYYY.txt
        // Ultimele 3 grupuri de cifre sunt mereu: Zi, Lună, An
        const yearA = parseInt(matchA[matchA.length - 1], 10);
        const monthA = parseInt(matchA[matchA.length - 2], 10) - 1; // Lunile în JS încep de la 0
        const dayA = parseInt(matchA[matchA.length - 3], 10);

        const yearB = parseInt(matchB[matchB.length - 1], 10);
        const monthB = parseInt(matchB[matchB.length - 2], 10) - 1;
        const dayB = parseInt(matchB[matchB.length - 3], 10);

        const dateA = new Date(yearA, monthA, dayA);
        const dateB = new Date(yearB, monthB, dayB);

        // Sortare descrescătoare: Cel mai NOU fișier trece SUS
        return dateB - dateA;
    });
}

function setupOverlayEvents() {
    const overlay = document.getElementById('logs-overlay');
    document.getElementById('btn-toggle-logs')?.addEventListener('click', () => {
        overlay.classList.add('open');
    });
    document.getElementById('btn-close-overlay')?.addEventListener('click', () => {
        overlay.classList.remove('open');
    });
}

function setupArrowNavigationEvents() {
    document.getElementById('btn-prev-day')?.addEventListener('click', () => navigateHistory(-1));
    document.getElementById('btn-next-day')?.addEventListener('click', () => navigateHistory(1));
}


function generateFriendlyTitle(filename) {
    const log_parts = filename.replace('.txt', '').split('_');
    if (log_parts.length >= 4) {
        const dateStr = `${log_parts[log_parts.length - 3]}-${log_parts[log_parts.length - 2]}-${log_parts[log_parts.length - 1]}`;
        if (filename.includes('dht')) return `Inside Temp/Humid (${dateStr})`;
        if (filename.includes('ds')) return `Water Temperature (${dateStr})`;
    }
    return filename;
}

function navigateHistory(direction) {
    if (!currentActiveFile) return;

    const isDhtList = currentActiveFile.includes('dht');
    const currentList = isDhtList ? allDhtFiles : allDsFiles;
    const currentIndex = currentList.indexOf(currentActiveFile);

    if (currentIndex === -1) return;

    const newIndex = currentIndex + direction;
    if (newIndex >= 0 && newIndex < currentList.length) {
        const targetFilename = currentList[newIndex];
        downloadAndDrawGoogleChart(targetFilename, generateFriendlyTitle(targetFilename));
    }
}

function updateArrowButtonsState() {
    const isDhtList = currentActiveFile.includes('dht');
    const currentList = isDhtList ? allDhtFiles : allDsFiles;
    const currentIndex = currentList.indexOf(currentActiveFile);

    const prevBtn = document.getElementById('btn-prev-day');
    const nextBtn = document.getElementById('btn-next-day');

    if (prevBtn && nextBtn && currentIndex !== -1) {
        prevBtn.disabled = (currentIndex === 0);
        nextBtn.disabled = (currentIndex === currentList.length - 1);
    }
}

// Preia lista de loguri de la ESP32 și o desparte în două coloane
function loadLogsManager() {
    const dhtContainer = document.getElementById('logs-list-dht');
    const dsContainer = document.getElementById('logs-list-ds');
    const infoContainer = document.getElementById('logs-list-info');
    const errContainer = document.getElementById('logs-list-errors');

    if (!dhtContainer || !dsContainer || !infoContainer || !errContainer) return;

    dhtContainer.innerHTML = dsContainer.innerHTML = infoContainer.innerHTML = errContainer.innerHTML = '';

    const appendStorageItem = (filename, folderName, targetContainer) => {
        const row = document.createElement('div');
        row.className = 'log-row-item';
        const link = document.createElement('span');
        link.className = 'log-link';
        link.innerText = `📄 ${filename}`;
        const isChartable = folderName === 'logs';

        if (isChartable) {
            const friendlyTitle = generateFriendlyTitle(filename);
            link.onclick = () => {
                downloadAndDrawGoogleChart(filename, friendlyTitle);
                document.getElementById('logs-overlay').classList.remove('open');
            };
        } else {

            // CORECTAT: În loc de window.open, apelăm funcția noastră de citire în overlay
            link.title = "Apasă pentru a citi fișierul direct aici";
            link.onclick = () => viewTextFileInline(folderName, filename);
        }

        const actionsContainer = document.createElement('div');
        actionsContainer.style.display = 'flex';
        actionsContainer.style.alignItems = 'center';

        const downBtn = document.createElement('a');
        downBtn.className = 'btn-download-log';
        downBtn.innerHTML = '📥';
        downBtn.href = `/${folderName}/${filename}`;
        downBtn.setAttribute('download', filename);
        downBtn.onclick = (e) => e.stopPropagation();

        const delBtn = document.createElement('button');
        delBtn.className = 'btn-delete-log';
        delBtn.innerHTML = '🗑️';
        delBtn.onclick = (e) => { e.stopPropagation(); deleteStorageFile(folderName, filename); };

        actionsContainer.appendChild(downBtn);
        actionsContainer.appendChild(delBtn);
        row.appendChild(link);
        row.appendChild(actionsContainer);
        targetContainer.appendChild(row);
    };

    // 1. Ordonare deșteaptă pentru /logs/
    fetch('/logs/')
        .then(res => res.json())
        .then(files => {
            // Folosim noua funcție pentru a ordona descrescător în interfață (Afișare sus cele noi)
            sortFilesByDateDescending(files);

            // Pentru navigarea din săgeți [◀]/[▶], ordinea din memorie trebuie să rămână cronologică normală (crescător)
            allDhtFiles = files.slice().reverse().filter(f => f.includes('dht_'));
            allDsFiles = files.slice().reverse().filter(f => f.includes('ds_'));

            if (files.length === 0) dhtContainer.innerHTML = dsContainer.innerHTML = '<p class="text-muted">Niciun log.</p>';

            files.forEach(f => {
                if (f.includes('dht_')) appendStorageItem(f, 'logs', dhtContainer);
                else if (f.includes('ds_')) appendStorageItem(f, 'logs', dsContainer);
            });

            // Auto-load fișier de azi
            const currentTime = new Date();
            const day = String(currentTime.getDate()).padStart(2, '0');
            const month = String(currentTime.getMonth() + 1).padStart(2, '0');
            const year = currentTime.getFullYear();
            const todayFilename = `dht_${day}_${month}_${year}.txt`;
            if (files.includes(todayFilename)) {
                downloadAndDrawGoogleChart(todayFilename, generateFriendlyTitle(todayFilename));
            } else if (allDhtFiles.length > 0) {
                // Dacă nu e log de azi, trage ultimul fișier din lista cronologică crescătoare
                downloadAndDrawGoogleChart(allDhtFiles[allDhtFiles.length - 1], generateFriendlyTitle(allDhtFiles[allDhtFiles.length - 1]));
            }
        }).catch(() => dhtContainer.innerHTML = '<p class="text-muted">Eroare logs</p>');

    // 2. Ordonare deșteaptă pentru /info/
    fetch('/info/')
        .then(res => res.json())
        .then(files => {
            if (files.length === 0) {
                infoContainer.innerHTML = '<p class="text-muted">Niciun fișier.</p>';
                return;
            }

            // CORECTARE: Sortare numerică reală după timp descrescător
            sortFilesByDateDescending(files);

            files.forEach(f => appendStorageItem(f, 'info', infoContainer));
        }).catch(() => infoContainer.innerHTML = '<p class="text-muted">Eroare info</p>');

    // 3. Ordonare deșteaptă pentru /errors/
    fetch('/errors/')
        .then(res => res.json())
        .then(files => {
            if (files.length === 0) {
                errContainer.innerHTML = '<p class="text-muted">Niciun fișier.</p>';
                return;
            }

            // CORECTARE: Sortare numerică reală după timp descrescător
            sortFilesByDateDescending(files);

            files.forEach(f => appendStorageItem(f, 'errors', errContainer));
        }).catch(() => errContainer.innerHTML = '<p class="text-muted">Eroare erori</p>');
}

// Actualizăm și funcțiile de interogare fișiere/ștergere să folosească ruta generală definită în C
function downloadAndDrawGoogleChart(filename, chartTitle) {
    currentActiveFile = filename;
    updateArrowButtonsState();

    // Înlocuit /charts/ cu /logs/ conform unificării tale structurale
    fetch(`/logs/${filename}`)
        .then(response => { if (!response.ok) throw new Error(); return response.text(); })
        .then(rawText => {
            const lines = rawText.split('\n');
            const isDHT = filename.includes('dht');
            const dataArray = isDHT ? [['Time', 'Temperature (°C)', 'Humidity (%)']] : [['Time', 'Water Temp (°C)']];

            lines.forEach(line => {
                const cleanLine = line.trim();
                if (!cleanLine || cleanLine.includes('invalid')) return;
                try {
                    const mainParts = cleanLine.split(" - ");
                    if (mainParts.length < 2) return;
                    const timeStamp = mainParts[0];
                    const dataPart = mainParts[1];
                    const tempParts = dataPart.split("C");
                    const temperature = parseFloat(tempParts[0]);

                    if (!isNaN(temperature)) {
                        if (isDHT && tempParts.length > 1) {
                            const humidRaw = tempParts[1].replace('RH', '').trim();
                            const humidity = parseFloat(humidRaw);
                            if (!isNaN(humidity)) dataArray.push([timeStamp, temperature, humidity]);
                        } else {
                            dataArray.push([timeStamp, temperature]);
                        }
                    }
                } catch (e) { }
            });

            if (typeof google !== 'undefined' && google.visualization) {
                lastChartDataTable = google.visualization.arrayToDataTable(dataArray);
                activeChartOptions = {
                    title: chartTitle, curveType: 'function',
                    legend: { position: 'bottom', textStyle: { color: '#e1e1e6' } },
                    backgroundColor: '#202024', titleTextStyle: { color: '#e1e1e6', fontSize: 14, bold: true },
                    hAxis: { textStyle: { color: '#8d8d99' }, gridlines: { color: '#29292e' } },
                    vAxis: { textStyle: { color: '#8d8d99' }, gridlines: { color: '#29292e' } },
                    colors: isDHT ? ['#f5a623', '#04d361'] : ['#4ba3e3']
                };
                new google.visualization.LineChart(document.querySelector('#curve_chart')).draw(lastChartDataTable, activeChartOptions);
            }
        });
}

// Funcția de ștergere acum acceptă dinamic numele folderului de proveniență
function deleteStorageFile(folderName, filename) {
    if (!confirm(`Sigur ștergi definitiv ${filename} din /${folderName}/?`)) return;

    fetch(`/delete/${folderName}/${filename}`, { method: 'POST' })
        .then(response => {
            if (response.ok) {
                if (currentActiveFile === filename) {
                    document.querySelector('#curve_chart').innerHTML = '';
                    lastChartDataTable = null;
                }
                loadLogsManager(); // Reîncarcă toate cele 4 coloane automat
            } else {
                alert("Eroare la ștergerea fișierului.");
            }
        });
}

// Buton mobil toggle
document.getElementById('menu-toggle')?.addEventListener('click', () => {
    const sidebar = document.getElementById('sidebar');

    // Toggle pe clasa 'open' (se deschide/închide din același buton)
    sidebar.classList.toggle('open');

    // Dacă sidebar-ul s-a deschis, blocăm scroll-ul pe fundal. Dacă s-a închis, îl permitem.
    if (sidebar.classList.contains('open')) {
        document.body.classList.add('no-scroll');
    } else {
        document.body.classList.remove('no-scroll');
    }
});

// ============================================================================
// LOGICĂ ACORDEON SIDEBAR ȘI COMUTATOARE DINAMICE LOGURI
// ============================================================================

window.addEventListener('DOMContentLoaded', () => {
    initAccordion();
    initDynamicLogsVisibility();
});

// 1. Managementul Acordeonului (Varianta A - Single Open)
function initAccordion() {
    const accordionItems = document.querySelectorAll('.accordion-item');

    // Deschidem prima secțiune implicit (General Timing) pentru a nu porni meniul complet strâns
    if (accordionItems.length > 0) {
        accordionItems[0].classList.add('active');
    }

    accordionItems.forEach(item => {
        const btn = item.querySelector('.accordion-btn');

        btn.addEventListener('click', () => {
            const isAlreadyActive = item.classList.contains('active');

            // Închidem absolut toate celelalte secțiuni active din sidebar (Regula Single Open)
            accordionItems.forEach(i => i.classList.remove('active'));

            // Dacă elementul pe care am dat click nu era deschis, îl deschidem acum
            if (!isAlreadyActive) {
                item.classList.add('active');
            }
        });
    });
}

// 2. Controlul vizibilității câmpurilor de interval (Ascundere dacă logarea e oprită)
function initDynamicLogsVisibility() {
    const dhtEnableSelect = document.getElementById('log_dht_enable');
    const dsEnableSelect = document.getElementById('log_ds_enable');

    const toggleDhtVisibility = () => {
        const dhtRow = document.getElementById('row-dht-interval');
        if (!dhtRow || !dhtEnableSelect) return;

        // "1" înseamnă activ, "0" înseamnă deactivat
        if (dhtEnableSelect.value === "1") {
            dhtRow.style.display = 'flex'; // Afișează normal câmpul de minute
        } else {
            dhtRow.style.display = 'none'; // Ascunde complet rândul din interfață
        }
    };

    const toggleDsVisibility = () => {
        const dsRow = document.getElementById('row-ds-interval');
        if (!dsRow || !dsEnableSelect) return;

        if (dsEnableSelect.value === "1") {
            dsRow.style.display = 'flex';
        } else {
            dsRow.style.display = 'none';
        }
    };

    // Ascultăm schimbările făcute manual de utilizator pe pagină
    dhtEnableSelect?.addEventListener('change', toggleDhtVisibility);
    dsEnableSelect?.addEventListener('change', toggleDsVisibility);

    // Rulăm funcțiile și la evenimentul onmessage (când sosesc datele inițiale "init" din ESP32)
    // Pentru a integra asta automat în sistemul tău, adăugăm un observer pe modificări
    const originalOnMessage = window.onmessage;

    // Extindem funcția ta onMessage existentă pentru a rula verificarea vizuală la fiecare update
    const originalCallback = typeof onMessage === 'function' ? onMessage : null;
    if (originalCallback) {
        // Creăm un mic wrapper peste funcția ta de onMessage din script.js ca să ruleze auto-hide-ul
        window.addEventListener('message_processed', () => {
            toggleDhtVisibility();
            toggleDsVisibility();
        });
    }

    // Alternativă sigură: rulăm verificarea periodic sau după ce s-a procesat JSON-ul
    setInterval(() => {
        toggleDhtVisibility();
        toggleDsVisibility();
    }, 400);
}


// Funcție care descarcă fișierul de info/errors în fundal și îl afișează în popup
// Înlocuiește această funcție în script.js:

function viewTextFileInline(folderName, filename) {
    const viewerOverlay = document.getElementById('text-viewer-overlay');
    const viewerContent = document.getElementById('text-viewer-content');
    const viewerTitle = document.getElementById('text-viewer-title');

    if (!viewerOverlay || !viewerContent || !viewerTitle) return;

    viewerTitle.innerText = `📄 /${folderName}/${filename}`;
    viewerContent.innerText = "Se încarcă și se ordonează datele...";
    viewerOverlay.style.display = "flex";

    fetch(`/${folderName}/${filename}`)
        .then(response => {
            if (!response.ok) throw new Error("Fișierul nu a putut fi citit");
            return response.text();
        })
        .then(textBrut => {
            if (!textBrut.trim()) {
                viewerContent.innerText = "Fișierul este gol.";
                return;
            }

            // 1. Spargem textul în linii independente
            const lines = textBrut.split('\n');

            // 2. Curățăm liniile de spații albe și eliminăm rândurile complet goale
            const cleanLines = lines.map(line => line.trim()).filter(line => line.length > 0);

            // 3. INVERSĂM liniile (Cele mai noi evenimente trec în VÂRF/SUS)
            cleanLines.reverse();

            // 4. Re-asamblăm liniile inversate într-un singur string unit prin linie nouă
            viewerContent.innerText = cleanLines.join('\n');
        })
        .catch(err => {
            viewerContent.innerText = `Eroare la citire: ${err.message}\nVerifică dacă fișierul mai există pe cardul SD.`;
        });
}


// Configurare buton de închidere sub-overlay text (Atașăm evenimentul la încărcare)
window.addEventListener('load', () => {
    document.getElementById('btn-close-viewer')?.addEventListener('click', () => {
        document.getElementById('text-viewer-overlay').style.display = "none";
    });
});

// ============================================================================
// LOGICĂ AVANSATĂ WI-FI: SCAN, TEST CONEXIUNE ȘI REDIRECȚIONARE SMART
// ============================================================================

window.addEventListener('DOMContentLoaded', () => {
    initWifiEvents();
});

function initWifiEvents() {
    const wifiModeSelect = document.getElementById('wifi_mode');
    const staSection = document.getElementById('section-sta-config');
    const btnScan = document.getElementById('btn-wifi-scan');
    const btnTest = document.getElementById('btn-wifi-test');
    const btnSwitch = document.getElementById('btn-wifi-switch');
    const statusBox = document.getElementById('wifi-status-box');
    const statusText = document.getElementById('wifi-status-text');
    const switchContainer = document.getElementById('wifi-switch-container');
    const ipDisplay = document.getElementById('wifi-ip-display');
    const scannedDropdown = document.getElementById('wifi_scanned_ssids');

    let dynamicAllocatedIP = ""; // Reține noul IP trimis de ESP32

    // 1. Afișează secțiunea de configurare STA doar dacă modul selectat este STA (1)
    const updateWifiUI = () => {
        if (wifiModeSelect && staSection) {
            staSection.style.display = (wifiModeSelect.value === "1") ? "flex" : "none";
        }
    };
    wifiModeSelect?.addEventListener('change', updateWifiUI);

    // Rulăm o verificare periodică sau la init pentru a deschide UI-ul dacă ESP e deja în mod STA
    setInterval(updateWifiUI, 1000);

    // 2. Comanda de Scanare Rețele (AJAX GET/POST către ESP32)
    btnScan?.addEventListener('click', (e) => {
        e.stopPropagation();
        if (!scannedDropdown) return;

        btnScan.innerText = "⏳";
        scannedDropdown.innerHTML = '<option value="">Se caută...</option>';

        fetch('/api/wifi/scan', { method: 'POST' })
            .then(res => { if (!res.ok) throw new Error(); return res.json(); })
            .then(networks => {
                scannedDropdown.innerHTML = '<option value="">Alege rețea</option>';
                networks.forEach(net => {
                    const opt = document.createElement('option');
                    opt.value = net.ssid;
                    opt.innerText = `${net.ssid} (${net.rssi}dBm)`;
                    scannedDropdown.appendChild(opt);
                });
                btnScan.innerText = "🔍";
            })
            .catch(() => {
                scannedDropdown.innerHTML = '<option value="">Eroare scan</option>';
                btnScan.innerText = "🔍";
            });
    });

    // 3. Comanda de Testare Conexiune (Trimite credențialele prin AJAX)
    btnTest?.addEventListener('click', (e) => {
        e.stopPropagation();
        const ssid = scannedDropdown?.value;
        const pass = document.getElementById('wifi_password')?.value || "";

        if (!ssid) {
            alert("Te rog selectează o rețea Wi-Fi din listă!");
            return;
        }

        if (statusBox && statusText && switchContainer) {
            statusBox.style.display = "flex";
            switchContainer.style.display = "none";
            statusText.innerText = "Se testează conexiunea... Așteaptă confirmarea de la router (max 15s).";
            statusText.style.color = "var(--text-muted)";
        }

        // Trimitem pachetul către o rută nouă dedicată verificării
        fetch('/api/wifi/test', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ssid: ssid, password: pass })
        })
            .then(res => { if (!res.ok) throw new Error(); return res.json(); })
            .then(result => {
                // Structura primită din C: { status: "connected"/"failed", ip: "10.0.10.202" }
                if (result.status === "connected" && result.ip) {
                    dynamicAllocatedIP = result.ip;
                    statusText.innerText = "✅ Parolă corectă! Conexiune reușită cu succes.";
                    statusText.style.color = "#04d361";
                    if (ipDisplay) ipDisplay.innerText = `IP Alocat: ${dynamicAllocatedIP}`;
                    switchContainer.style.display = "flex"; // Afișăm butonul de Switch!
                } else {
                    statusText.innerText = "❌ Conexiune eșuată! Verifică dacă parola este corectă.";
                    statusText.style.color = "#c53030";
                    switchContainer.style.display = "none";
                }
            })
            .catch(() => {
                if (statusText) {
                    statusText.innerText = "❌ Eroare comunicare server la testare.";
                    statusText.style.color = "#c53030";
                }
            });
    });

    // 4. Comanda de SWITCH FINALE (Închide AP și Redirecționează Browserul)
    btnSwitch?.addEventListener('click', (e) => {
        e.stopPropagation();
        if (!dynamicAllocatedIP) return;

        if (!confirm("Sigur vrei să comuți în mod STA? Interfața AP se va închide, iar sistemul se va muta pe rețeaua casei.")) return;

        if (statusText && switchContainer) {
            switchContainer.style.display = "none";
            statusText.innerHTML = `⚠️ Se închide modul AP...<br>Conectează-ți telefonul la Wi-Fi-ul casei.<br>Redirecționare automată în <span id="wifi-countdown">12</span> secunde...`;
            statusText.style.color = "#f5a623";
        }

        // Trimitem comanda de execuție finală către ESP32
        fetch('/api/wifi/switch', { method: 'POST' });

        // Pornim numărătoarea inversă pentru redirecționare smart
        let secondsLeft = 12;
        const countdownInterval = setInterval(() => {
            secondsLeft--;
            const countEl = document.getElementById('wifi-countdown');
            if (countEl) countEl.innerText = secondsLeft;

            if (secondsLeft <= 0) {
                clearInterval(countdownInterval);
                // Aruncăm utilizatorul direct pe noul URL/IP primit de la router
                window.location.href = `http://${dynamicAllocatedIP}`;
            }
        }, 1000);
    });
}



