/*
!! L'automate génère 2 ports COM. Imposez le bon !
*/
#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>

// ========== CONFIGURATION ETHERNET FILAIRE ==========
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05 }; 
IPAddress ip(192, 168, 50, 50);

EthernetServer webServer(80);
EthernetClient ethClient;          
ModbusTCPClient modbusTCPClient(ethClient); 

#define MAX_AUTOCLAVES 10
int nbAutoclaves = 5;

IPAddress ipAutoclaves[MAX_AUTOCLAVES] = {
  IPAddress(192, 168, 50, 51),
  IPAddress(192, 168, 50, 52),
  IPAddress(192, 168, 50, 53),
  IPAddress(192, 168, 50, 54),
  IPAddress(192, 168, 50, 55)
};

// LA FAMEUSE CONSIGNE GLOBALE POUR TOUT LE MONDE
float consigneGlobale = 110.0;
int etatMachine[MAX_AUTOCLAVES] = {0};
float tempMachine[MAX_AUTOCLAVES] = {0.0};
bool enLigne[MAX_AUTOCLAVES] = {false};

// --- MOTEUR DE DIAGNOSTIC BRUT (Logs & ASCII) ---
bool hasNewError = false;
String lastErrorTitle = "";
String lastDiagnostic = "";

void pingAndSync(int id) {
  modbusTCPClient.setTimeout(250); 
  
  if (modbusTCPClient.begin(ipAutoclaves[id], 502)) {
    enLigne[id] = true;
    
    // FIX MODBUS : Écriture groupée (Fonction 16) pour ne pas saturer l'ESP32
    // On écrit 2 registres d'un coup en partant de l'adresse 0x01
    modbusTCPClient.beginTransmission(1, HOLDING_REGISTERS, 0x01, 2);
    modbusTCPClient.write(etatMachine[id]);                       // Registre 0x01
    modbusTCPClient.write((uint16_t)(consigneGlobale * 10));      // Registre 0x02
    modbusTCPClient.endTransmission();
    
    delay(20); // On laisse 20ms à l'écran TFT de l'ESP32 pour se rafraîchir
    
    // Lecture de la température (Fonction 03)
    long tempBrute = modbusTCPClient.holdingRegisterRead(1, 0x00);
    if (tempBrute != -1) tempMachine[id] = tempBrute / 10.0;
    
    modbusTCPClient.stop();
  } else {
    enLigne[id] = false;
    tempMachine[id] = 0.0;
    modbusTCPClient.stop();
    ethClient.stop();

    String ipStr = String(ipAutoclaves[id][0]) + "." + String(ipAutoclaves[id][1]) + "." + String(ipAutoclaves[id][2]) + "." + String(ipAutoclaves[id][3]);
    float ts = millis() / 1000.0; 
    
    hasNewError = true;
    lastErrorTitle = "syslog: connection timeout to " + ipStr;
    
    lastDiagnostic =  " __________________________________________________ \n";
    lastDiagnostic += "|    ___   ___                                     |\n";
    lastDiagnostic += "|   | _ \\ | _ \\    RAYNAL & ROQUELAURE             |\n";
    lastDiagnostic += "|   |   / |   /                                    |\n";
    lastDiagnostic += "|   |_|_\\ |_|_\\    LYCEE CHARLES CARNUS            |\n";
    lastDiagnostic += "|__________________________________________________|\n\n";
    
    lastDiagnostic += "[ " + String(ts, 3) + " ] modbus_tcp[502]: WARN: polling node " + ipStr + "...\n";
    lastDiagnostic += "[ " + String(ts + 0.250, 3) + " ] modbus_tcp[502]: ERROR: connection timeout to " + ipStr + ":502\n";
    lastDiagnostic += "[ " + String(ts + 0.251, 3) + " ] scada_daemon[110]: kernel: state transition node_" + String(id) + " -> FAULT\n";
    lastDiagnostic += "[ " + String(ts + 0.251, 3) + " ] scada_daemon[110]: dropping node from active polling pool\n";
    lastDiagnostic += "[ " + String(ts + 0.252, 3) + " ] opta_watchdog[42]: CHECK TRACE TRIGGERED:\n";
    lastDiagnostic += "   - verify 24V PSU on remote node\n";
    lastDiagnostic += "   - verify PHY link status (Switch LEDs)\n";
    lastDiagnostic += "   - verify cabling (RJ45 pair mismatch)\n";
    lastDiagnostic += "   - verify emergency stop loop status\n";
    lastDiagnostic += "[ " + String(ts + 0.255, 3) + " ] opta_watchdog[42]: waiting for manual ACK or reconnect\n";
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== OPTA : SUPERVISION SCADA INDUS ===");
  Ethernet.begin(mac, ip);
  delay(1000);
  Serial.print("IP OPTA: ");
  Serial.println(Ethernet.localIP());
  webServer.begin();
}

void loop() {
  EthernetClient webClient = webServer.available();
  if (webClient) {
    String request = "";
    boolean currentLineIsBlank = true; 

    while (webClient.connected()) {
      if (webClient.available()) {
        char c = webClient.read();
        request += c;
        
        if (c == '\n' && currentLineIsBlank) {
          
          if (request.indexOf("GET /?bg_update=1") >= 0) {
            for(int i = 0; i < nbAutoclaves; i++) {
              if(enLigne[i]) pingAndSync(i); 
            }
          }
          else if (request.indexOf("GET /?scan=") >= 0) {
            int id = request.substring(request.indexOf("scan=") + 5, request.indexOf("scan=") + 7).toInt();
            if(id < nbAutoclaves) pingAndSync(id);
          }
          else if (request.indexOf("GET /?start=") >= 0) {
            int id = request.substring(request.indexOf("start=") + 6, request.indexOf("start=") + 8).toInt();
            if(id < nbAutoclaves) { etatMachine[id] = 1; pingAndSync(id); }
          }
          else if (request.indexOf("GET /?stop=") >= 0) {
            int id = request.substring(request.indexOf("stop=") + 5, request.indexOf("stop=") + 7).toInt();
            if(id < nbAutoclaves) { etatMachine[id] = 0; pingAndSync(id); }
          }
          else if (request.indexOf("GET /?consigne=") >= 0) {
            int posDebut = request.indexOf("consigne=") + 9;
            int posFin = request.indexOf(" HTTP", posDebut); 
            consigneGlobale = request.substring(posDebut, posFin).toFloat();
            // L'OPTA impose la consigne à TOUS les ESP en ligne !
            for(int i = 0; i < nbAutoclaves; i++) { if(enLigne[i]) pingAndSync(i); }
          }
          else if (request.indexOf("GET /?add_ip=") >= 0) {
            int posDebut = request.indexOf("add_ip=") + 7;
            int posFin = request.indexOf(" HTTP", posDebut);
            String newIpStr = request.substring(posDebut, posFin);
            if (nbAutoclaves < MAX_AUTOCLAVES) {
              IPAddress newIP;
              if (newIP.fromString(newIpStr)) { 
                ipAutoclaves[nbAutoclaves] = newIP;
                etatMachine[nbAutoclaves] = 0;
                pingAndSync(nbAutoclaves); 
                nbAutoclaves++;
              }
            }
          }
          else if (request.indexOf("GET /?del=") >= 0) {
            int posDebut = request.indexOf("del=") + 4;
            int posFin = request.indexOf(" HTTP", posDebut);
            int id = request.substring(posDebut, posFin).toInt();
            if (id >= 0 && id < nbAutoclaves) {
              for (int i = id; i < nbAutoclaves - 1; i++) {
                ipAutoclaves[i] = ipAutoclaves[i+1];
                etatMachine[i] = etatMachine[i+1];
                tempMachine[i] = tempMachine[i+1];
                enLigne[i] = enLigne[i+1];
              }
              nbAutoclaves--;
            }
          }

          // --- GÉNÉRATION HTML SCADA ---
          webClient.println("HTTP/1.1 200 OK");
          webClient.println("Content-Type: text/html; charset=utf-8");
          webClient.println("Connection: close");
          webClient.println();
          
          webClient.println("<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>");
          webClient.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
          webClient.println("<title>SCADA - Raynal & Roquelaure</title>");
          webClient.println("<style>");
          
          webClient.println(":root { --bg: #0f1012; --panel: #1a1b1e; --border: #2d2e32; --text: #e1e2e6; --red: #ff3333; --red-dark: #8b0000; --green: #00e676; --green-dark: #004d26; --yellow: #ffb800; --carnus: #005ce6; }");
          webClient.println("body { background: var(--bg); color: var(--text); font-family: system-ui, -apple-system, sans-serif; margin: 0; padding: 20px; }");
          
          webClient.println(".supra-title { margin: 0 0 5px 0; color: var(--carnus); font-weight: 700; font-size: 0.85em; letter-spacing: 2px; text-transform: uppercase; }");
          webClient.println(".header { background: #0a0a0c; border-left: 6px solid var(--carnus); border-right: 6px solid var(--red); padding: 20px 25px; display: flex; justify-content: space-between; align-items: center; border-radius: 8px; border-bottom: 1px solid var(--border); margin-bottom: 25px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }");
          webClient.println(".header h1 { margin: 0; font-size: 1.5em; color: #fff; letter-spacing: 2px; text-transform: uppercase; font-weight: 700; }");
          webClient.println(".header .status { color: var(--green); font-family: 'Courier New', monospace; font-size: 1.2em; font-weight: bold; }");
          
          webClient.println(".controls { display: flex; gap: 20px; margin-bottom: 25px; flex-wrap: wrap; }");
          webClient.println(".control-box { background: var(--panel); border: 1px solid var(--border); padding: 20px; flex: 1; border-radius: 8px; min-width: 300px; box-shadow: 0 4px 10px rgba(0,0,0,0.2); }");
          webClient.println(".control-box h3 { margin: 0 0 15px 0; font-size: 0.95em; color: var(--yellow); text-transform: uppercase; letter-spacing: 1px; }");
          webClient.println(".input-group { display: flex; gap: 10px; }");
          webClient.println("input { background: #000; border: 1px solid #444; color: #fff; padding: 10px; font-family: 'Courier New', monospace; font-size: 1.1em; flex: 1; text-align: center; border-radius: 4px; outline: none; transition: border 0.3s; }");
          webClient.println("input:focus { border-color: var(--carnus); }");
          webClient.println("button { background: #2a2b30; color: #fff; border: 1px solid #444; padding: 10px 20px; cursor: pointer; font-weight: 600; text-transform: uppercase; border-radius: 4px; transition: all 0.2s ease; display: flex; justify-content: center; align-items: center; gap: 8px; }");
          webClient.println("button:hover:not(:disabled) { background: #3f4045; border-color: #666; transform: translateY(-1px); }");
          webClient.println("button:disabled { opacity: 0.7; cursor: not-allowed; }");
          
          webClient.println(".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 20px; }");
          webClient.println(".card { background: var(--panel); border: 1px solid var(--border); padding: 20px; display: flex; flex-direction: column; gap: 12px; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.3); position: relative; overflow: hidden; }");
          webClient.println(".card::before { content: ''; position: absolute; top: 0; left: 0; width: 100%; height: 4px; }");
          webClient.println(".card.online::before { background: var(--green); box-shadow: 0 0 10px var(--green); }");
          webClient.println(".card.offline::before { background: var(--red); box-shadow: 0 0 10px var(--red); }");
          
          webClient.println(".card-title { font-weight: 800; color: #fff; display: flex; justify-content: space-between; align-items: center; font-size: 1.1em; }");
          webClient.println(".card-ip { font-family: 'Courier New', monospace; color: #777; font-size: 0.9em; margin-bottom: 5px; }");
          webClient.println(".data-display { background: #0a0a0c; padding: 20px 15px; border: 1px inset #222; text-align: center; border-radius: 6px; }");
          webClient.println(".val-temp { font-family: 'Courier New', monospace; font-size: 3em; font-weight: bold; color: var(--green); text-shadow: 0 0 15px rgba(0,255,102,0.2); }");
          webClient.println(".val-temp.err { color: var(--red); text-shadow: 0 0 15px rgba(255,51,51,0.2); }");
          
          webClient.println(".btn-sys { width: 100%; margin-top: 8px; font-size: 0.85em; letter-spacing: 0.5px; }");
          webClient.println(".btn-sys.start { background: var(--green-dark); border-color: var(--green); color: #fff; } .btn-sys.start:hover:not(:disabled) { background: var(--green); color: #000; }");
          webClient.println(".btn-sys.stop { background: var(--red-dark); border-color: var(--red); color: #fff; } .btn-sys.stop:hover:not(:disabled) { background: var(--red); color: #fff; }");
          
          webClient.println(".toast { position: fixed; top: 20px; right: 20px; background: #1a0505; border-left: 6px solid var(--red); padding: 20px; width: 320px; display: none; box-shadow: 0 10px 30px rgba(255,51,51,0.2); z-index: 1000; border-radius: 6px; animation: slideIn 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275) forwards; }");
          webClient.println("@keyframes slideIn { from { transform: translateX(120%); opacity: 0; } to { transform: translateX(0); opacity: 1; } }");
          webClient.println(".toast h4 { margin: 0 0 12px 0; color: #fff; font-size: 1.1em; display: flex; align-items: center; gap: 8px; }");
          webClient.println(".toast h4::before { content: '⚠️'; color: var(--red); }");
          webClient.println(".toast button { width: 100%; background: transparent; color: var(--yellow); border: 1px solid var(--yellow); }");
          webClient.println(".toast button:hover { background: rgba(241,196,15,0.1); }");
          
          webClient.println(".modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.85); display: none; justify-content: center; align-items: center; z-index: 2000; backdrop-filter: blur(4px); }");
          webClient.println(".modal-content { background: #0a0a0c; border: 1px solid #333; width: 90%; max-width: 800px; padding: 25px; border-radius: 8px; box-shadow: 0 20px 50px rgba(0,0,0,0.5); }");
          webClient.println(".modal h3 { color: #fff; margin: 0 0 20px 0; display: flex; justify-content: space-between; align-items: center; font-family: monospace;}");
          webClient.println(".modal textarea { width: 100%; height: 350px; background: #050505; color: #d4d4d4; font-family: 'Courier New', monospace; padding: 15px; border: 1px solid #222; border-radius: 4px; resize: none; font-size: 0.95em; line-height: 1.5; box-sizing: border-box; white-space: pre; overflow-wrap: normal; }");
          webClient.println(".close-btn { background: transparent; color: #888; border: none; font-size: 1.5em; cursor: pointer; padding: 0; } .close-btn:hover { color: #fff; }");
          
          webClient.println(".spinner { display: none; width: 14px; height: 14px; border: 2px solid rgba(255,255,255,0.3); border-top-color: #fff; border-radius: 50%; animation: spin 0.8s linear infinite; }");
          webClient.println(".loading .spinner { display: inline-block; }");
          webClient.println("@keyframes spin { 100% { transform: rotate(360deg); } }");
          
          webClient.println("</style>");
          
          webClient.println("<script>");
          webClient.println("setInterval(() => {");
          webClient.println("  if(document.querySelectorAll('.loading').length === 0 && document.getElementById('modal').style.display !== 'flex') {");
          webClient.println("    fetch('/?bg_update=1').then(r=>r.text()).then(html=>{");
          webClient.println("      let parser = new DOMParser(); let doc = parser.parseFromString(html, 'text/html');");
          webClient.println("      document.getElementById('machines-grid').innerHTML = doc.getElementById('machines-grid').innerHTML;");
          webClient.println("      checkErrors(doc);");
          webClient.println("    }).catch(e=>{});");
          webClient.println("  }");
          webClient.println("}, 2000);");
          
          webClient.println("function sendReq(btn, url) {");
          webClient.println("  btn.classList.add('loading'); btn.disabled = true;");
          webClient.println("  fetch(url).then(r=>r.text()).then(html=>{");
          webClient.println("    let parser = new DOMParser(); let doc = parser.parseFromString(html, 'text/html');");
          webClient.println("    document.getElementById('machines-grid').innerHTML = doc.getElementById('machines-grid').innerHTML;");
          webClient.println("    checkErrors(doc);");
          webClient.println("  }).catch(e=>{ alert('ERREUR CRITIQUE RÉSEAU'); btn.classList.remove('loading'); btn.disabled = false; });");
          webClient.println("}");
          
          webClient.println("function sendForm(e, form, prefix) {");
          webClient.println("  e.preventDefault();");
          webClient.println("  let btn = form.querySelector('button'); btn.classList.add('loading'); btn.disabled = true;");
          webClient.println("  let val = form.querySelector('input').value;");
          webClient.println("  fetch(prefix + val).then(r=>r.text()).then(html=>{");
          webClient.println("    let parser = new DOMParser(); let doc = parser.parseFromString(html, 'text/html');");
          webClient.println("    document.getElementById('machines-grid').innerHTML = doc.getElementById('machines-grid').innerHTML;");
          webClient.println("    checkErrors(doc); btn.classList.remove('loading'); btn.disabled = false;");
          webClient.println("  });");
          webClient.println("}");

          webClient.println("function checkErrors(doc) {");
          webClient.println("  if(doc.getElementById('sys-err').innerText === '1') {");
          webClient.println("    document.getElementById('t-title').innerText = doc.getElementById('sys-title').innerText;");
          webClient.println("    document.getElementById('log-text').value = doc.getElementById('sys-diag').innerText;");
          webClient.println("    document.getElementById('toast').style.display = 'block';");
          webClient.println("  }");
          webClient.println("}");
          
          webClient.println("function showLogs() { document.getElementById('toast').style.display = 'none'; document.getElementById('modal').style.display = 'flex'; }");
          webClient.println("function closeLogs() { document.getElementById('modal').style.display = 'none'; }");
          webClient.println("</script></head><body>");

          webClient.println("<div style='display:none;' id='sys-err'>" + String(hasNewError ? "1" : "0") + "</div>");
          webClient.println("<div style='display:none;' id='sys-title'>" + lastErrorTitle + "</div>");
          webClient.println("<div style='display:none;' id='sys-diag'>" + lastDiagnostic + "</div>");
          hasNewError = false;

          webClient.println("<div id='toast' class='toast'><h4 id='t-title'>Erreur</h4><button onclick='showLogs()'>VOIR LE DIAGNOSTIC</button></div>");
          webClient.println("<div id='modal' class='modal'><div class='modal-content'><h3><span>>_ root@opta-master:~# tail -f /var/log/syslog</span><button class='close-btn' onclick='closeLogs()'>&times;</button></h3><textarea id='log-text' readonly></textarea></div></div>");

          webClient.println("<div class='header'><div><p class='supra-title'>Supervisory Control And Data Acquisition</p><h1>SCADA // RAYNAL & ROQUELAURE</h1></div><div class='status'>SYS.OK _</div></div>");
          
          webClient.println("<div class='controls'>");
          webClient.println("<div class='control-box'><h3>Consigne Réseau (SV)</h3><form onsubmit='sendForm(event, this, \"/?consigne=\")' class='input-group'>");
          webClient.print("<input type='number' step='0.1' value='");
          webClient.print(consigneGlobale, 1);
          webClient.println("'> <button type='submit'><span>WR_REG</span><div class='spinner'></div></button></form></div>");
          
          webClient.println("<div class='control-box'><h3>Déployer Nœud</h3>");
          if (nbAutoclaves < MAX_AUTOCLAVES) {
            webClient.println("<form onsubmit='sendForm(event, this, \"/?add_ip=\")' class='input-group'><input type='text' placeholder='IP (ex: .56)' required> <button type='submit'><span>CONNECT</span><div class='spinner'></div></button></form>");
          } else { webClient.println("<p style='color:var(--red); font-weight:bold;'>OVERLOAD</p>"); }
          webClient.println("</div></div>");

          webClient.println("<div id='machines-grid' class='grid'>");
          for(int i = 0; i < nbAutoclaves; i++) {
            String ipStr = String(ipAutoclaves[i][0]) + "." + String(ipAutoclaves[i][1]) + "." + String(ipAutoclaves[i][2]) + "." + String(ipAutoclaves[i][3]);
            String cClass = enLigne[i] ? "card online" : "card offline";
            
            webClient.println("<div class='" + cClass + "'>");
            webClient.println("<div class='card-title'><span>AUTOCLAVE " + String(i+1) + "</span> <span style='color:" + (enLigne[i] ? "var(--green)" : "var(--red)") + "'>●</span></div>");
            webClient.println("<div class='card-ip'>TCP // " + ipStr + "</div>");
            
            if (enLigne[i]) {
              webClient.println("<div class='data-display'><div class='val-temp'>" + String(tempMachine[i], 1) + "</div><div style='color:#666; font-size:0.8em; letter-spacing:1px;'>CELSIUS (PV)</div></div>");
              
              if (etatMachine[i] == 1) {
                webClient.println("<button onclick='sendReq(this, \"/?stop=" + String(i) + "\")' class='btn-sys stop'><span>[0] STOP CYCLE</span><div class='spinner'></div></button>");
              } else {
                webClient.println("<button onclick='sendReq(this, \"/?start=" + String(i) + "\")' class='btn-sys start'><span>[1] START CYCLE</span><div class='spinner'></div></button>");
              }
            } else {
              webClient.println("<div class='data-display'><div class='val-temp err'>ERR</div><div style='color:#666; font-size:0.8em; letter-spacing:1px;'>COMM FAULT</div></div>");
              webClient.println("<button onclick='sendReq(this, \"/?scan=" + String(i) + "\")' class='btn-sys start' style='background:#003366; border-color:#004b9b;'><span>PING NODE</span><div class='spinner'></div></button>");
            }
            webClient.println("<button onclick='sendReq(this, \"/?del=" + String(i) + "\")' class='btn-sys' style='background:transparent; color:#666; border-color:#333;'><span>DROP NODE</span><div class='spinner'></div></button>");
            webClient.println("</div>");
          }
          webClient.println("</div>"); 
          
          webClient.println("</body></html>");
          delay(10); 
          break;
        }
        
        if (c == '\n') { currentLineIsBlank = true; } 
        else if (c != '\r') { currentLineIsBlank = false; }
      }
    }
    webClient.stop(); 
  }
}