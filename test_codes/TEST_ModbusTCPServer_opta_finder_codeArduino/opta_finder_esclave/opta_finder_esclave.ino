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

float consigneGlobale = 110.0;
int etatMachine[MAX_AUTOCLAVES] = {0};
float tempMachine[MAX_AUTOCLAVES] = {0.0};
bool enLigne[MAX_AUTOCLAVES] = {false};

// --- MOTEUR DE DIAGNOSTIC ---
bool hasNewError = false;
String lastErrorTitle = "";
String lastDiagnostic = "";

void pingAndSync(int id) {
  modbusTCPClient.setTimeout(250); 
  
  if (modbusTCPClient.begin(ipAutoclaves[id], 502)) {
    enLigne[id] = true;
    modbusTCPClient.holdingRegisterWrite(0x02, (uint16_t)(consigneGlobale * 10));
    modbusTCPClient.holdingRegisterWrite(0x01, etatMachine[id]);
    long tempBrute = modbusTCPClient.holdingRegisterRead(0x00);
    if (tempBrute != -1) tempMachine[id] = tempBrute / 10.0;
    modbusTCPClient.stop();
  } else {
    // GENERATION DE L'ERREUR INDUSTRIELLE
    enLigne[id] = false;
    tempMachine[id] = 0.0;
    modbusTCPClient.stop();
    ethClient.stop();

    String ipStr = String(ipAutoclaves[id][0]) + "." + String(ipAutoclaves[id][1]) + "." + String(ipAutoclaves[id][2]) + "." + String(ipAutoclaves[id][3]);
    
    hasNewError = true;
    lastErrorTitle = "Échec Modbus TCP - Autoclave " + String(id+1);
    
    // Formatage d'un Log de maintenance robuste
    lastDiagnostic = "====================================\n";
    lastDiagnostic += "RAPPORT DE DIAGNOSTIC - RAYNAL & ROQUELAURE\n";
    lastDiagnostic += "Timestamp (ms) : " + String(millis()) + "\n";
    lastDiagnostic += "Code Erreur    : ERR_TCP_TIMEOUT\n";
    lastDiagnostic += "Cible IP       : " + ipStr + " (Port 502)\n";
    lastDiagnostic += "====================================\n\n";
    lastDiagnostic += "[ANALYSE]\n";
    lastDiagnostic += "L'automate maitre (OPTA) n'a pas reçu de reponse Modbus \n";
    lastDiagnostic += "de l'esclave dans le delai imparti (250ms).\n\n";
    lastDiagnostic += "[PROCEDURE DE MAINTENANCE HORS-LIGNE]\n";
    lastDiagnostic += "1. Verifier la presence de tension 24V sur l'Autoclave " + String(id+1) + ".\n";
    lastDiagnostic += "2. Controler l'etat de la LED 'Link' sur le switch Ethernet.\n";
    lastDiagnostic += "3. Remplacer le cable RJ45 si le port clignote orange.\n";
    lastDiagnostic += "4. S'assurer que le bouton d'Arret d'Urgence n'est pas enclenche.\n";
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
          
          // --- TRAITEMENT DES ORDRES AJAX ---
          if (request.indexOf("GET /?scan=") >= 0) {
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
          webClient.println("<title>SCADA - Raynal & Roquelaure</title>");
          webClient.println("<style>");
          
          // CSS INDUSTRIEL (Brut, Contraste, Monospace)
          webClient.println(":root { --bg: #121212; --panel: #1e1e1e; --border: #333; --text: #d4d4d4; --red: #e74c3c; --green: #00ff66; --yellow: #f1c40f; --carnus: #004b9b; }");
          webClient.println("body { background: var(--bg); color: var(--text); font-family: 'Segoe UI', sans-serif; margin: 0; padding: 15px; }");
          webClient.println(".header { background: #000; border-left: 5px solid var(--carnus); border-right: 5px solid var(--red); padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid var(--border); margin-bottom: 20px; }");
          webClient.println(".header h1 { margin: 0; font-size: 1.4em; color: #fff; letter-spacing: 1px; }");
          webClient.println(".header .status { color: var(--green); font-family: monospace; font-size: 1.1em; }");
          
          webClient.println(".controls { display: flex; gap: 15px; margin-bottom: 20px; }");
          webClient.println(".control-box { background: var(--panel); border: 1px solid var(--border); padding: 15px; flex: 1; }");
          webClient.println(".control-box h3 { margin: 0 0 10px 0; font-size: 1em; color: var(--yellow); text-transform: uppercase; }");
          webClient.println("input { background: #000; border: 1px solid #555; color: #fff; padding: 8px; font-family: monospace; font-size: 1.1em; width: 140px; text-align: center; }");
          webClient.println("button { background: #333; color: #fff; border: 1px solid #555; padding: 8px 15px; cursor: pointer; font-weight: bold; text-transform: uppercase; transition: 0.2s; }");
          webClient.println("button:hover { background: #444; border-color: #777; }");
          
          webClient.println(".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 15px; }");
          webClient.println(".card { background: var(--panel); border: 1px solid var(--border); padding: 15px; display: flex; flex-direction: column; gap: 10px; }");
          webClient.println(".card.online { border-top: 3px solid var(--green); }");
          webClient.println(".card.offline { border-top: 3px solid var(--red); }");
          
          webClient.println(".card-title { font-weight: bold; color: #fff; display: flex; justify-content: space-between; }");
          webClient.println(".card-ip { font-family: monospace; color: #888; font-size: 0.9em; }");
          webClient.println(".data-display { background: #000; padding: 15px; border: 1px inset #222; text-align: center; margin: 10px 0; }");
          webClient.println(".val-temp { font-family: 'Courier New', monospace; font-size: 2.5em; font-weight: bold; color: var(--green); }");
          webClient.println(".val-temp.err { color: var(--red); }");
          
          webClient.println(".btn-sys { width: 100%; margin-top: 5px; }");
          webClient.println(".btn-sys.start { background: #005a2b; border-color: var(--green); } .btn-sys.start:hover { background: var(--green); color: #000; }");
          webClient.println(".btn-sys.stop { background: #5a0000; border-color: var(--red); } .btn-sys.stop:hover { background: var(--red); color: #fff; }");
          
          // --- NOTIFICATION TOAST & MODAL LOG ---
          webClient.println(".toast { position: fixed; top: 20px; right: 20px; background: #5a0000; border: 2px solid var(--red); padding: 15px; width: 300px; display: none; box-shadow: 0 0 20px rgba(231,76,60,0.4); z-index: 1000; }");
          webClient.println(".toast h4 { margin: 0 0 10px 0; color: #fff; }");
          webClient.println(".toast button { width: 100%; background: #000; color: var(--yellow); border-color: var(--yellow); }");
          
          webClient.println(".modal { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.8); display: none; justify-content: center; align-items: center; z-index: 2000; }");
          webClient.println(".modal-content { background: #111; border: 2px solid #555; width: 80%; max-width: 800px; padding: 20px; }");
          webClient.println(".modal textarea { width: 100%; height: 300px; background: #000; color: var(--green); font-family: 'Courier New', monospace; padding: 10px; border: 1px solid #333; resize: none; }");
          webClient.println(".modal .close-btn { background: var(--red); color: white; float: right; margin-bottom: 10px; }");
          
          // Loader
          webClient.println(".spinner { display: inline-block; width: 12px; height: 12px; border: 2px solid #fff; border-top-color: transparent; border-radius: 50%; animation: spin 1s linear infinite; margin-left: 5px; }");
          webClient.println("@keyframes spin { 100% { transform: rotate(360deg); } }");
          
          webClient.println("</style>");
          
          // --- JAVASCRIPT AJAX & ERROR HANDLING ---
          webClient.println("<script>");
          webClient.println("function sendReq(btn, url) {");
          webClient.println("  let oldTxt = btn.innerHTML; btn.innerHTML = 'WAIT <div class=\"spinner\"></div>'; btn.disabled = true;");
          webClient.println("  fetch(url).then(r=>r.text()).then(html=>{");
          webClient.println("    let parser = new DOMParser(); let doc = parser.parseFromString(html, 'text/html');");
          webClient.println("    document.getElementById('dashboard').innerHTML = doc.getElementById('dashboard').innerHTML;");
          webClient.println("    checkErrors(doc);");
          webClient.println("  }).catch(e=>alert('CRITICAL: OPTA INJOIGNABLE'));");
          webClient.println("}");
          
          webClient.println("function sendForm(e, form, prefix) {");
          webClient.println("  e.preventDefault();");
          webClient.println("  let btn = form.querySelector('button'); let oldTxt = btn.innerHTML; btn.innerHTML = '... <div class=\"spinner\"></div>';");
          webClient.println("  let val = form.querySelector('input').value;");
          webClient.println("  fetch(prefix + val).then(r=>r.text()).then(html=>{");
          webClient.println("    let parser = new DOMParser(); let doc = parser.parseFromString(html, 'text/html');");
          webClient.println("    document.getElementById('dashboard').innerHTML = doc.getElementById('dashboard').innerHTML;");
          webClient.println("    checkErrors(doc); btn.innerHTML = oldTxt;");
          webClient.println("  });");
          webClient.println("}");

          webClient.println("function checkErrors(doc) {");
          webClient.println("  let errFlag = doc.getElementById('sys-err').innerText;");
          webClient.println("  if(errFlag === '1') {");
          webClient.println("    document.getElementById('t-title').innerText = doc.getElementById('sys-title').innerText;");
          webClient.println("    document.getElementById('log-text').value = doc.getElementById('sys-diag').innerText;");
          webClient.println("    document.getElementById('toast').style.display = 'block';");
          webClient.println("  }");
          webClient.println("}");
          
          webClient.println("function showLogs() { document.getElementById('toast').style.display = 'none'; document.getElementById('modal').style.display = 'flex'; }");
          webClient.println("function closeLogs() { document.getElementById('modal').style.display = 'none'; }");
          webClient.println("</script></head><body>");

          // --- DONNÉES CACHÉES POUR LE JS ---
          webClient.println("<div style='display:none;' id='sys-err'>" + String(hasNewError ? "1" : "0") + "</div>");
          webClient.println("<div style='display:none;' id='sys-title'>" + lastErrorTitle + "</div>");
          webClient.println("<div style='display:none;' id='sys-diag'>" + lastDiagnostic + "</div>");
          hasNewError = false; // On reset l'erreur après l'avoir envoyée

          // --- IHM DES ERREURS ---
          webClient.println("<div id='toast' class='toast'><h4 id='t-title'>Erreur</h4><button onclick='showLogs()'>[+] DÉTAILS DIAGNOSTIC</button></div>");
          webClient.println("<div id='modal' class='modal'><div class='modal-content'><button class='close-btn' onclick='closeLogs()'>X FERMER</button><h3>CONSOLE DE LOGS SYSTEME</h3><textarea id='log-text' readonly></textarea></div></div>");

          // --- INTERFACE PRINCIPALE ---
          webClient.println("<div class='header'><div><h1>SCADA // RAYNAL & ROQUELAURE</h1></div><div class='status'>SYS.OK _</div></div>");
          
          webClient.println("<div class='controls'>");
          webClient.println("<div class='control-box'><h3>Consigne Réseau</h3><form onsubmit='sendForm(event, this, \"/?consigne=\")'>");
          webClient.print("<input type='number' step='0.1' value='");
          webClient.print(consigneGlobale, 1);
          webClient.println("'> <button type='submit'>WR_REG</button></form></div>");
          
          webClient.println("<div class='control-box'><h3>Déployer Nœud</h3>");
          if (nbAutoclaves < MAX_AUTOCLAVES) {
            webClient.println("<form onsubmit='sendForm(event, this, \"/?add_ip=\")'><input type='text' placeholder='IP (ex: .56)' required> <button type='submit'>CONNECT</button></form>");
          } else { webClient.println("<p style='color:var(--red);'>OVERLOAD</p>"); }
          webClient.println("</div></div>");

          // --- DASHBOARD DYNAMIQUE (Mis à jour par AJAX) ---
          webClient.println("<div id='dashboard'><div class='grid'>");
          
          for(int i = 0; i < nbAutoclaves; i++) {
            String ipStr = String(ipAutoclaves[i][0]) + "." + String(ipAutoclaves[i][1]) + "." + String(ipAutoclaves[i][2]) + "." + String(ipAutoclaves[i][3]);
            String cClass = enLigne[i] ? "card online" : "card offline";
            
            webClient.println("<div class='" + cClass + "'>");
            webClient.println("<div class='card-title'><span>AUTOCLAVE " + String(i+1) + "</span> <span style='color:" + (enLigne[i] ? "var(--green)" : "var(--red)") + "'>●</span></div>");
            webClient.println("<div class='card-ip'>" + ipStr + "</div>");
            
            if (enLigne[i]) {
              webClient.println("<div class='data-display'><div class='val-temp'>" + String(tempMachine[i], 1) + "</div><div style='color:#888; font-size:0.8em;'>CELSIUS</div></div>");
              
              if (etatMachine[i] == 1) {
                webClient.println("<button onclick='sendReq(this, \"/?stop=" + String(i) + "\")' class='btn-sys stop'>[0] STOP CYCLE</button>");
              } else {
                webClient.println("<button onclick='sendReq(this, \"/?start=" + String(i) + "\")' class='btn-sys start'>[1] START CYCLE</button>");
              }
            } else {
              webClient.println("<div class='data-display'><div class='val-temp err'>ERR</div><div style='color:#888; font-size:0.8em;'>COMM FAULT</div></div>");
              webClient.println("<button onclick='sendReq(this, \"/?scan=" + String(i) + "\")' class='btn-sys start'>PING NODE</button>");
            }
            webClient.println("<button onclick='sendReq(this, \"/?del=" + String(i) + "\")' class='btn-sys' style='background:#111; color:#666;'>DROP NODE</button>");
            webClient.println("</div>");
          }
          
          webClient.println("</div></div></body></html>");
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