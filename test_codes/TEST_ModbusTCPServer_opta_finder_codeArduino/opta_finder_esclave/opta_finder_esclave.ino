/*
!! L'automate génère 2 ports COM. Imposez le bon !
*/
#include <Arduino.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>

// ========== CONFIGURATION ETHERNET FILAIRE (RJ45 UNIQUEMENT) ==========
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05 }; 
IPAddress ip(192, 168, 50, 50);

EthernetServer webServer(80);
EthernetClient ethClient;          
ModbusTCPClient modbusTCPClient(ethClient); 

// --- Paramètres de Scalabilité (Mise à l'échelle) ---
#define MAX_AUTOCLAVES 10
int nbAutoclaves = 5; // On commence avec les 5 de base

// Tableaux de données dimensionnés pour le maximum
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

// --- Fonction "Ping" et Synchronisation d'une machine ---
void pingAndSync(int id) {
  // Timeout très court (250ms) : agit comme un Ping. Si ça bloque, la machine est éteinte.
  modbusTCPClient.setTimeout(250); 
  
  if (modbusTCPClient.begin(ipAutoclaves[id], 502)) {
    enLigne[id] = true;
    
    // 1. Envoi de la consigne globale
    modbusTCPClient.holdingRegisterWrite(0x02, (uint16_t)(consigneGlobale * 10));
    // 2. Envoi de l'état (Marche/Arrêt) spécifique à cette machine
    modbusTCPClient.holdingRegisterWrite(0x01, etatMachine[id]);
    // 3. Lecture de la température
    long tempBrute = modbusTCPClient.holdingRegisterRead(0x00);
    if (tempBrute != -1) {
      tempMachine[id] = tempBrute / 10.0;
    }
    
    modbusTCPClient.stop();
  } else {
    enLigne[id] = false;
    tempMachine[id] = 0.0;
    modbusTCPClient.stop();
    ethClient.stop();
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== OPTA : SUPERVISION MANUELLE & DYNAMIQUE (RJ45) ===");
  
  // Lancement de la connexion physique Ethernet
  Ethernet.begin(mac, ip);
  delay(1000);
  Serial.print("IP OPTA (Ethernet): ");
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
          
          // =========================================================
          // 1. TRAITEMENT DES ORDRES UTILISATEUR
          // =========================================================
          
          // Action : Tester (Ping) une machine
          if (request.indexOf("GET /?scan=") >= 0) {
            int id = request.substring(request.indexOf("scan=") + 5, request.indexOf("scan=") + 7).toInt();
            if(id < nbAutoclaves) pingAndSync(id);
          }
          // Action : Démarrer une machine
          else if (request.indexOf("GET /?start=") >= 0) {
            int id = request.substring(request.indexOf("start=") + 6, request.indexOf("start=") + 8).toInt();
            if(id < nbAutoclaves) {
              etatMachine[id] = 1;
              pingAndSync(id);
            }
          }
          // Action : Arrêter une machine
          else if (request.indexOf("GET /?stop=") >= 0) {
            int id = request.substring(request.indexOf("stop=") + 5, request.indexOf("stop=") + 7).toInt();
            if(id < nbAutoclaves) {
              etatMachine[id] = 0;
              pingAndSync(id);
            }
          }
          // Action : Changer la consigne globale
          else if (request.indexOf("GET /?consigne=") >= 0) {
            int posDebut = request.indexOf("consigne=") + 9;
            int posFin = request.indexOf(" HTTP", posDebut); 
            consigneGlobale = request.substring(posDebut, posFin).toFloat();
            // On synchronise uniquement les machines qui sont déjà en ligne
            for(int i = 0; i < nbAutoclaves; i++) {
              if(enLigne[i]) pingAndSync(i);
            }
          }
          // Action : Ajouter une nouvelle adresse IP
          else if (request.indexOf("GET /?add_ip=") >= 0) {
            int posDebut = request.indexOf("add_ip=") + 7;
            int posFin = request.indexOf(" HTTP", posDebut);
            String newIpStr = request.substring(posDebut, posFin);
            
            // Si on n'a pas atteint la limite des 10 autoclaves
            if (nbAutoclaves < MAX_AUTOCLAVES) {
              IPAddress newIP;
              if (newIP.fromString(newIpStr)) { // Vérifie que c'est bien une IP valide
                ipAutoclaves[nbAutoclaves] = newIP;
                etatMachine[nbAutoclaves] = 0;
                pingAndSync(nbAutoclaves); // On la ping immédiatement !
                nbAutoclaves++;
              }
            }
          }
          // Action : Supprimer une IP
          else if (request.indexOf("GET /?del=") >= 0) {
            int posDebut = request.indexOf("del=") + 4;
            int posFin = request.indexOf(" HTTP", posDebut);
            int id = request.substring(posDebut, posFin).toInt();
            
            if (id >= 0 && id < nbAutoclaves) {
              // On décale toutes les machines suivantes vers la gauche
              for (int i = id; i < nbAutoclaves - 1; i++) {
                ipAutoclaves[i] = ipAutoclaves[i+1];
                etatMachine[i] = etatMachine[i+1];
                tempMachine[i] = tempMachine[i+1];
                enLigne[i] = enLigne[i+1];
              }
              nbAutoclaves--;
            }
          }

          // =========================================================
          // 2. GENERATION DYNAMIQUE DE LA PAGE HTML
          // =========================================================
          webClient.println("HTTP/1.1 200 OK");
          webClient.println("Content-Type: text/html; charset=utf-8");
          webClient.println("Connection: close");
          webClient.println();
          
          webClient.println("<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>");
          webClient.println("<title>Raynal & Roquelaure - Supervision Sécurisée</title>");
          webClient.println("<style>");
          webClient.println("body { background-color: #1a1a1a; color: #fff; font-family: 'Segoe UI', sans-serif; padding: 20px; text-align: center; }");
          webClient.println(".container { max-width: 1200px; margin: 0 auto; }");
          webClient.println(".header { background-color: #c8102e; padding: 20px; border-radius: 10px; border-bottom: 4px solid #f1c40f; margin-bottom: 20px;}");
          webClient.println("h1 { margin: 0; color: white; letter-spacing: 2px; }");
          webClient.println(".flex-panels { display: flex; justify-content: center; gap: 20px; flex-wrap: wrap; margin-bottom: 20px; }");
          webClient.println(".panel { background: #2d2d2d; padding: 20px; border-radius: 10px; border: 1px solid #444; min-width: 300px; }");
          webClient.println(".machines-grid { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; }");
          webClient.println(".machine-card { background: #222; border: 1px solid #444; border-radius: 10px; padding: 20px; width: 220px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); }");
          webClient.println(".temp-val { font-size: 2.2em; font-weight: bold; margin: 10px 0; color: #e74c3c; }");
          webClient.println(".btn { padding: 10px 15px; text-decoration: none; color: white; border-radius: 5px; display: inline-block; font-weight: bold; width: 100%; box-sizing: border-box; margin-bottom: 10px;}");
          webClient.println(".btn-start { background: #27ae60; } .btn-stop { background: #c0392b; } .btn-ping { background: #2980b9; } .btn-add { background: #8e44ad; border: none; cursor: pointer;}");
          webClient.println(".btn-del { background: #555; padding: 5px 10px; margin-top: 15px; font-size: 0.85em; } .btn-del:hover { background: #e74c3c; }");
          webClient.println("input[type=number], input[type=text] { padding: 8px; font-size: 1.1em; text-align: center; border-radius: 5px; border: none; margin-bottom: 10px; width: 80%; }");
          webClient.println("button { padding: 10px 15px; background: #f39c12; color: white; border: none; font-weight: bold; cursor: pointer; border-radius: 5px; width: 80%; }");
          webClient.println("</style></head><body><div class='container'>");
          
          webClient.println("<div class='header'><h1>RAYNAL & ROQUELAURE</h1>");
          webClient.println("<p style='color: #f1c40f; font-weight: bold;'>SUPERVISION MANUELLE & SCALABLE (RESEAU ETHERNET ISOLÉ)</p></div>");
          
          webClient.println("<div class='flex-panels'>");
          
          webClient.println("<div class='panel'><h3>Consigne Globale (°C)</h3>");
          webClient.println("<form action='/' method='GET'>");
          webClient.print("<input type='number' name='consigne' value='");
          webClient.print(consigneGlobale, 1);
          webClient.println("' step='0.1'><br><button type='submit'>APPLIQUER</button>");
          webClient.println("</form></div>");

          webClient.println("<div class='panel'><h3>Ajouter un Autoclave</h3>");
          if (nbAutoclaves < MAX_AUTOCLAVES) {
            webClient.println("<form action='/' method='GET'>");
            webClient.println("<input type='text' name='add_ip' placeholder='ex: 192.168.50.56' required><br>");
            webClient.println("<button type='submit' class='btn-add'>INTÉGRER & PING</button>");
            webClient.println("</form>");
          } else {
            webClient.println("<p style='color: #e74c3c; font-weight: bold;'>Capacité maximale atteinte (10)</p>");
          }
          webClient.println("</div></div>");

          webClient.println("<div class='machines-grid'>");
          
          for(int i = 0; i < nbAutoclaves; i++) {
            String ipStr = String(ipAutoclaves[i][0]) + "." + String(ipAutoclaves[i][1]) + "." + String(ipAutoclaves[i][2]) + "." + String(ipAutoclaves[i][3]);
            
            webClient.println("<div class='machine-card'>");
            webClient.println("<h3 style='color: #f1c40f; margin-top:0;'>Autoclave " + String(i+1) + "</h3>");
            webClient.println("<p style='margin: 5px 0; color: #aaa;'>IP: " + ipStr + "</p>");
            
            if (enLigne[i]) {
              webClient.println("<p style='color:#2ecc71; font-weight:bold; margin: 5px 0;'>✓ EN LIGNE</p>");
              webClient.println("<div class='temp-val'>" + String(tempMachine[i], 1) + " °C</div>");
              
              if (etatMachine[i] == 1) {
                webClient.println("<p style='color:#f39c12; font-weight:bold;'>EN CHAUFFE</p>");
                webClient.println("<a href='/?stop=" + String(i) + "' class='btn btn-stop'>ARRÊTER</a>");
              } else {
                webClient.println("<p style='color:#95a5a6; font-weight:bold;'>À L'ARRÊT</p>");
                webClient.println("<a href='/?start=" + String(i) + "' class='btn btn-start'>DÉMARRER</a>");
              }
              webClient.println("<a href='/?scan=" + String(i) + "' class='btn btn-ping'>ACTUALISER</a>");
            } else {
              webClient.println("<p style='color:#c0392b; font-weight:bold; margin: 5px 0;'>✗ HORS LIGNE</p>");
              webClient.println("<div class='temp-val' style='color:#7f8c8d'>--.- °C</div>");
              webClient.println("<a href='/?scan=" + String(i) + "' class='btn btn-ping'>TESTER (PING)</a>");
            }
            
            webClient.println("<a href='/?del=" + String(i) + "' class='btn btn-del'>🗑️ SUPPRIMER</a>");
            webClient.println("</div>");
          }
          
          webClient.println("</div></div></body></html>");
          
          delay(10); 
          break;
        }
        
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    webClient.stop(); 
  }
}