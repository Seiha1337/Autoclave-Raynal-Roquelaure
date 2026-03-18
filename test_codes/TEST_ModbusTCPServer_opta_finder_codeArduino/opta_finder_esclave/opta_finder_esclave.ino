/*
!! L'automate génère 2 ports COM
VsCode prend par défaut le mauvais
imposez le bon avant de transférer le code
*/

#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>

// ========== CONFIGURATION ETHERNET (IP FIXE) ==========
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05 }; 
IPAddress ip(192, 168, 50, 210);                      

// ========== SERVEURS ==========
EthernetServer webServer(80);     // Serveur web sur le port 80
EthernetServer modbusServer(502); // Serveur Modbus sur le port 502
ModbusTCPServer modbusTCPServer;

// ========== VARIABLES D'ÉTAT ==========
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Variables Modbus / Simulation
unsigned long lastUpdate = 0;
int fausseTemperature = 200; // 20.0 °C

// Variables anti-blocage (millis) pour les LEDs D0-D3
unsigned long previousMillisLED = 0;
int ledStep = 0;

// ========== PAGE HTML/CSS EMBARQUÉE (DESIGN R&R) ==========
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Supervision Autoclave - Raynal & Roquelaure</title>
    <meta http-equiv="refresh" content="2">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #1a1a1a; color: #ffffff; min-height: 100vh; padding: 20px; }
        .container { max-width: 900px; margin: 0 auto; }
        
        /* Bandeau Raynal & Roquelaure */
        .header { background-color: #c8102e; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 15px rgba(200, 16, 46, 0.3); border-bottom: 4px solid #f1c40f; }
        h1 { color: white; text-align: center; margin-bottom: 5px; font-size: 2.2em; letter-spacing: 2px; text-transform: uppercase; }
        .subtitle { text-align: center; color: #f1c40f; font-size: 1.2em; font-weight: bold; }
        
        /* Cartes d'information */
        .card { background: #2d2d2d; border: 1px solid #444; border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 5px 15px rgba(0,0,0,0.5); }
        .card h3 { color: #f1c40f; margin-bottom: 15px; font-size: 1.3em; text-align: center; border-bottom: 1px solid #444; padding-bottom: 10px; }
        
        /* Section Modbus */
        .modbus-grid { display: flex; gap: 20px; margin-bottom: 20px; }
        .modbus-box { flex: 1; background: #222; border-radius: 8px; padding: 15px; text-align: center; border: 1px solid #555; }
        .modbus-label { color: #aaa; font-size: 0.9em; text-transform: uppercase; margin-bottom: 5px; }
        .modbus-value { font-size: 2.5em; font-weight: bold; color: #fff; }
        .val-temp { color: #e74c3c; }
        .val-cons { color: #2ecc71; }
        
        .info-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; color: #ccc; }
        .info-grid p { margin: 5px 0; }
        .info-grid strong { color: #fff; }

        /* Grilles Relais et Entrées */
        .relays-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; }
        .relay-card { background: #222; border-radius: 10px; padding: 15px; text-align: center; border: 1px solid #444; }
        .relay-card h4 { color: #fff; margin-bottom: 15px; }
        .relay-buttons { display: flex; gap: 10px; margin-bottom: 10px; }
        .relay-state { margin-top: 10px; padding: 5px; border-radius: 5px; font-weight: bold; }
        
        .btn { flex: 1; padding: 10px; font-size: 0.9em; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; text-decoration: none; color: white; text-align: center; }
        .btn-on { background: #27ae60; }
        .btn-on:hover { background: #2ecc71; }
        .btn-off { background: #c0392b; }
        .btn-off:hover { background: #e74c3c; }
        
        .inputs-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(80px, 1fr)); gap: 10px; }
        .input-item { text-align: center; padding: 10px; border-radius: 8px; background: #1a1a1a; border: 1px solid #333; }
        .input-label { font-weight: bold; color: #aaa; margin-bottom: 5px; }
        .input-state { font-size: 1.5em; margin-top: 5px; }
        .state-high { color: #2ecc71; text-shadow: 0 0 10px #2ecc71; }
        .state-low { color: #555; }
    </style>
</head>
<body>
    <div class="container">
        
        <div class="header">
            <h1>RAYNAL & ROQUELAURE</h1>
            <p class="subtitle">SUPERVISION AUTOCLAVE N°1 - OPTA</p>
        </div>
        
        <div class="modbus-grid">
            <div class="modbus-box">
                <div class="modbus-label">Température Cuve</div>
                <div class="modbus-value val-temp">TEMP_PLACEHOLDER °C</div>
            </div>
            <div class="modbus-box">
                <div class="modbus-label">Consigne Cible</div>
                <div class="modbus-value val-cons">CONSIGNE_PLACEHOLDER °C</div>
            </div>
        </div>

        <div class="card">
            <h3>Diagnostic Réseau</h3>
            <div class="info-grid">
                <p><strong>📡 Liaison :</strong> Ethernet Filaire (RJ45)</p>
                <p><strong>🔌 IP Fixe :</strong> IP_PLACEHOLDER</p>
                <p><strong>⚙️ Plateforme :</strong> Automate OPTA</p>
                <p><strong>🌐 Ports ouverts :</strong> 502 (Modbus) & 80 (Web)</p>
            </div>
        </div>
        
        <div class="card">
            <h3>Actionneurs (Relais)</h3>
            <div class="relays-grid">
                <div class="relay-card">
                    <h4>Vanne Vapeur (R1)</h4>
                    <div class="relay-buttons">
                        <a href="/RELAY1=ON" class="btn btn-on">ON</a>
                        <a href="/RELAY1=OFF" class="btn btn-off">OFF</a>
                    </div>
                    <div class="relay-state RELAY1_CLASS">RELAY1_STATE</div>
                </div>
                <div class="relay-card">
                    <h4>Vanne Eau (R2)</h4>
                    <div class="relay-buttons">
                        <a href="/RELAY2=ON" class="btn btn-on">ON</a>
                        <a href="/RELAY2=OFF" class="btn btn-off">OFF</a>
                    </div>
                    <div class="relay-state RELAY2_CLASS">RELAY2_STATE</div>
                </div>
                <div class="relay-card">
                    <h4>Vidange (R3)</h4>
                    <div class="relay-buttons">
                        <a href="/RELAY3=ON" class="btn btn-on">ON</a>
                        <a href="/RELAY3=OFF" class="btn btn-off">OFF</a>
                    </div>
                    <div class="relay-state RELAY3_CLASS">RELAY3_STATE</div>
                </div>
                <div class="relay-card">
                    <h4>Alarme (R4)</h4>
                    <div class="relay-buttons">
                        <a href="/RELAY4=ON" class="btn btn-on">ON</a>
                        <a href="/RELAY4=OFF" class="btn btn-off">OFF</a>
                    </div>
                    <div class="relay-state RELAY4_CLASS">RELAY4_STATE</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h3>Capteurs (Entrées Digitales)</h3>
            <div class="inputs-grid">
                <div class="input-item">
                    <div class="input-label">I1</div>
                    <div class="input-state INPUT1_CLASS">INPUT1_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I2</div>
                    <div class="input-state INPUT2_CLASS">INPUT2_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I3</div>
                    <div class="input-state INPUT3_CLASS">INPUT3_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I4</div>
                    <div class="input-state INPUT4_CLASS">INPUT4_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I5</div>
                    <div class="input-state INPUT5_CLASS">INPUT5_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I6</div>
                    <div class="input-state INPUT6_CLASS">INPUT6_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I7</div>
                    <div class="input-state INPUT7_CLASS">INPUT7_STATE</div>
                </div>
                <div class="input-item">
                    <div class="input-label">I8</div>
                    <div class="input-state INPUT8_CLASS">INPUT8_STATE</div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== Arduino OPTA - Serveur Web + Modbus (ETHERNET) ===");
  
  // Configuration LED
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_D0, OUTPUT); pinMode(LED_D1, OUTPUT);
  pinMode(LED_D2, OUTPUT); pinMode(LED_D3, OUTPUT);
  pinMode(LEDR, OUTPUT); pinMode(LEDG, OUTPUT);
  
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_D0, LOW); digitalWrite(LED_D1, LOW);
  digitalWrite(LED_D2, LOW); digitalWrite(LED_D3, LOW);
  digitalWrite(LEDR, LOW); digitalWrite(LEDG, LOW);   
  
  // Configuration bouton USER & Relais & Entrées
  pinMode(BTN_USER, INPUT_PULLUP);
  
  pinMode(RELAY1, OUTPUT); pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT); pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY1, LOW); digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW); digitalWrite(RELAY4, LOW);
  
  pinMode(I1, INPUT); pinMode(I2, INPUT); pinMode(I3, INPUT); pinMode(I4, INPUT);
  pinMode(I5, INPUT); pinMode(I6, INPUT); pinMode(I7, INPUT); pinMode(I8, INPUT);
  
  // Connexion Ethernet Filaire
  Serial.println("Initialisation du reseau Ethernet...");
  Ethernet.begin(mac, ip);
  delay(1000);
  
  Serial.println("\n✓ Reseau OK !");
  Serial.print("Adresse IP: ");
  Serial.println(Ethernet.localIP());
  
  // Démarrage des serveurs
  webServer.begin();
  modbusServer.begin();
  
  if (!modbusTCPServer.begin()) {
    Serial.println("✗ Erreur initialisation Modbus !");
    digitalWrite(LEDR, HIGH);  // Rouge ON = Erreur
  } else {
    modbusTCPServer.configureHoldingRegisters(0x00, 3);
    modbusTCPServer.holdingRegisterWrite(0x00, fausseTemperature); // Reg 0 : Température
    modbusTCPServer.holdingRegisterWrite(0x01, 1);                 // Reg 1 : Etat
    modbusTCPServer.holdingRegisterWrite(0x02, 1100);              // Reg 2 : Consigne par défaut (110.0 °C)
    Serial.println("✓ Serveur Modbus démarre (Port 502)");
    digitalWrite(LEDG, HIGH);  // Vert ON = OK
  }
  
  Serial.println("✓ Serveur web démarre (Port 80)");
  Serial.println("=============================\n");
}

void loop() {
  unsigned long currentMillis = millis();

  // ---------------------------------------------------------
  // 1. ANIMATION DES LEDs
  // ---------------------------------------------------------
  if (currentMillis - previousMillisLED >= 100) {
    previousMillisLED = currentMillis;
    digitalWrite(LED_D0, LOW); digitalWrite(LED_D1, LOW);
    digitalWrite(LED_D2, LOW); digitalWrite(LED_D3, LOW);
    
    if(ledStep == 0) digitalWrite(LED_D0, HIGH);
    else if(ledStep == 1) digitalWrite(LED_D1, HIGH);
    else if(ledStep == 2) digitalWrite(LED_D2, HIGH);
    else if(ledStep == 3) digitalWrite(LED_D3, HIGH);
    
    ledStep++;
    if (ledStep > 4) ledStep = 0; 
  }

  // ---------------------------------------------------------
  // 2. GESTION DU MODBUS TCP (qModMaster)
  // ---------------------------------------------------------
  EthernetClient modbusClient = modbusServer.available();
  if (modbusClient) {
    modbusTCPServer.accept(modbusClient);
    if (modbusClient.connected()) {
      modbusTCPServer.poll();
    }
  }

  // ---------------------------------------------------------
  // 3. MISE A JOUR DES DONNEES (La simulation Modbus)
  // ---------------------------------------------------------
  if (currentMillis - lastUpdate > 1000) {
    lastUpdate = currentMillis;
    fausseTemperature += 5; // +0.5 °C
    if (fausseTemperature > 1100) fausseTemperature = 200;
    modbusTCPServer.holdingRegisterWrite(0x00, fausseTemperature);
  }

  // ---------------------------------------------------------
  // 4. GESTION DU BOUTON USER
  // ---------------------------------------------------------
  int buttonReading = digitalRead(BTN_USER);
  if (buttonReading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }
  
  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (buttonReading == LOW) {  
      Serial.println("Test des relais depuis le bouton...");
      digitalWrite(RELAY1, !digitalRead(RELAY1));
      while(digitalRead(BTN_USER) == LOW) { delay(10); } 
    }
  }
  lastButtonState = buttonReading;

  // ---------------------------------------------------------
  // 5. GESTION DU SERVEUR WEB
  // ---------------------------------------------------------
  EthernetClient webClient = webServer.available();
  if (webClient) {
    String currentLine = "";
    String request = "";
    
    while (webClient.connected()) {
      if (webClient.available()) {
        char c = webClient.read();
        request += c;
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            
            // Traitement des boutons HTML
            if (request.indexOf("GET /RELAY1=ON") >= 0) { digitalWrite(RELAY1, HIGH); }
            else if (request.indexOf("GET /RELAY1=OFF") >= 0) { digitalWrite(RELAY1, LOW); }
            else if (request.indexOf("GET /RELAY2=ON") >= 0) { digitalWrite(RELAY2, HIGH); }
            else if (request.indexOf("GET /RELAY2=OFF") >= 0) { digitalWrite(RELAY2, LOW); }
            else if (request.indexOf("GET /RELAY3=ON") >= 0) { digitalWrite(RELAY3, HIGH); }
            else if (request.indexOf("GET /RELAY3=OFF") >= 0) { digitalWrite(RELAY3, LOW); }
            else if (request.indexOf("GET /RELAY4=ON") >= 0) { digitalWrite(RELAY4, HIGH); }
            else if (request.indexOf("GET /RELAY4=OFF") >= 0) { digitalWrite(RELAY4, LOW); }
            
            // Envoi de l'en-tête HTTP
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-Type: text/html; charset=utf-8");
            webClient.println("Connection: close");
            webClient.println();
            
            // Formatage de l'adresse IP
            String ipStr = String(Ethernet.localIP()[0]) + "." + 
                           String(Ethernet.localIP()[1]) + "." + 
                           String(Ethernet.localIP()[2]) + "." + 
                           String(Ethernet.localIP()[3]);

            // LECTURE DE LA CONSIGNE DIRECTEMENT DEPUIS LE MODBUS !
            long consigneBrute = modbusTCPServer.holdingRegisterRead(0x02);
            float consigneReelle = consigneBrute / 10.0;

            // Remplacement des Placeholders HTML
            String page = webpage;
            page.replace("IP_PLACEHOLDER", ipStr);
            page.replace("TEMP_PLACEHOLDER", String(fausseTemperature / 10.0, 1));
            page.replace("CONSIGNE_PLACEHOLDER", String(consigneReelle, 1)); // Injection de la consigne
            
            page.replace("INPUT1_STATE", digitalRead(I1) ? "●" : "○"); page.replace("INPUT1_CLASS", digitalRead(I1) ? "state-high" : "state-low");
            page.replace("INPUT2_STATE", digitalRead(I2) ? "●" : "○"); page.replace("INPUT2_CLASS", digitalRead(I2) ? "state-high" : "state-low");
            page.replace("INPUT3_STATE", digitalRead(I3) ? "●" : "○"); page.replace("INPUT3_CLASS", digitalRead(I3) ? "state-high" : "state-low");
            page.replace("INPUT4_STATE", digitalRead(I4) ? "●" : "○"); page.replace("INPUT4_CLASS", digitalRead(I4) ? "state-high" : "state-low");
            page.replace("INPUT5_STATE", digitalRead(I5) ? "●" : "○"); page.replace("INPUT5_CLASS", digitalRead(I5) ? "state-high" : "state-low");
            page.replace("INPUT6_STATE", digitalRead(I6) ? "●" : "○"); page.replace("INPUT6_CLASS", digitalRead(I6) ? "state-high" : "state-low");
            page.replace("INPUT7_STATE", digitalRead(I7) ? "●" : "○"); page.replace("INPUT7_CLASS", digitalRead(I7) ? "state-high" : "state-low");
            page.replace("INPUT8_STATE", digitalRead(I8) ? "●" : "○"); page.replace("INPUT8_CLASS", digitalRead(I8) ? "state-high" : "state-low");
            
            page.replace("RELAY1_STATE", digitalRead(RELAY1) ? "ON" : "OFF"); page.replace("RELAY1_CLASS", digitalRead(RELAY1) ? "state-high" : "state-low");
            page.replace("RELAY2_STATE", digitalRead(RELAY2) ? "ON" : "OFF"); page.replace("RELAY2_CLASS", digitalRead(RELAY2) ? "state-high" : "state-low");
            page.replace("RELAY3_STATE", digitalRead(RELAY3) ? "ON" : "OFF"); page.replace("RELAY3_CLASS", digitalRead(RELAY3) ? "state-high" : "state-low");
            page.replace("RELAY4_STATE", digitalRead(RELAY4) ? "ON" : "OFF"); page.replace("RELAY4_CLASS", digitalRead(RELAY4) ? "state-high" : "state-low");
            
            webClient.print(page);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    webClient.stop();
  }
}