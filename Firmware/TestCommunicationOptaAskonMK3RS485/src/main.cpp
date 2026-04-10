#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>

// ============================================================
// 🌐 COMMUNICATION RESEAU (Modbus TCP)
// ============================================================
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05 }; 
IPAddress ip(172, 40, 1, 50); 
EthernetServer ethServer(502); 

ModbusTCPServer modbusTCPServer;

// ============================================================
// 🔌 COMMUNICATION ASCON KM3 (Modbus RTU)
// ============================================================
constexpr int KM3_ADDRESS = 4;
constexpr auto baudrate { 9600 };
constexpr auto bitduration { 1.f / baudrate };
constexpr auto preDelayBR { bitduration * 9.6f * 3.5f * 1e6 };
constexpr auto postDelayBR { bitduration * 9.6f * 3.5f * 1e6 };

long consignePrecedente = -1;
unsigned long previousMillis = 0;
const unsigned long INTERVALLE_SCRUTATION = 2000; 

void setup() {
  pinMode(LED_D0, OUTPUT);
  pinMode(LED_D1, OUTPUT);
  digitalWrite(LED_D0, HIGH);

  Serial.begin(9600);
  delay(2000); 

  Serial.println("==========================================");
  Serial.println(" 🚀 DEMARRAGE HYBRIDE : MODBUS TCP + USB");
  Serial.println("==========================================");

  // 1. Démarrage Réseau
  Ethernet.begin(mac, ip);
  ethServer.begin();
  Serial.print(" [RESEAU] Connecté ! IP : ");
  Serial.println(Ethernet.localIP());

  if (!modbusTCPServer.begin()) {
    Serial.println(" ❌ Erreur Serveur Modbus TCP");
    while (1);
  }
  modbusTCPServer.configureHoldingRegisters(0, 3);
  Serial.println(" [TCP] Serveur Modbus en écoute sur le port 502...");

  // 2. Démarrage Série RTU
  RS485.setDelays(preDelayBR, postDelayBR);
  if (!ModbusRTUClient.begin(baudrate, SERIAL_8N1)) {
    Serial.println(" ❌ Erreur Client Modbus RTU");
    while (1);
  }
  Serial.println(" [RTU] Câble série RS485 prêt !");
  Serial.println("==========================================");
}

void loop() {
  digitalWrite(LED_D1, !digitalRead(LED_D1)); 

  // ---------------------------------------------------------
  // 🌐 CANAL 1 : COMMANDE VIA LE RÉSEAU (Site Web Nathan)
  // ---------------------------------------------------------
  EthernetClient client = ethServer.accept();
  if (client) {
    modbusTCPServer.accept(client);
  }
  modbusTCPServer.poll();

  long consigneWeb = modbusTCPServer.holdingRegisterRead(2);

  // Si l'ordre vient du réseau
  if (consigneWeb != consignePrecedente && consignePrecedente != -1) {
    Serial.print("\n🌐 [RESEAU] Commande TCP reçue : ");
    Serial.println(consigneWeb);

    if (ModbusRTUClient.holdingRegisterWrite(KM3_ADDRESS, 0x0006, consigneWeb)) {
      Serial.println("    >>> 🏭 [SUCCÈS] Consigne transmise au régulateur !");
      consignePrecedente = consigneWeb; 
    }
  }

  // ---------------------------------------------------------
  // ⌨️ CANAL 2 : COMMANDE VIA LE CLAVIER (Câble USB)
  // ---------------------------------------------------------
  if (Serial.available() > 0) {
    String saisie = Serial.readStringUntil('\n');
    saisie.trim();
    int nouvelleConsigne = saisie.toInt();

    if (saisie.length() > 0 && (nouvelleConsigne != 0 || saisie == "0")) {
      Serial.print("\n⌨️ [LOCAL] Commande Clavier/USB reçue : ");
      Serial.println(nouvelleConsigne);

      if (ModbusRTUClient.holdingRegisterWrite(KM3_ADDRESS, 0x0006, nouvelleConsigne)) {
        Serial.println("    >>> 🏭 [SUCCÈS] Consigne transmise au régulateur !");
        
        // 🔄 SYNCHRONISATION : On met à jour la boîte mémoire TCP pour que 
        // le Node.js de supervision voit aussi ce changement manuel !
        modbusTCPServer.holdingRegisterWrite(2, nouvelleConsigne);
        consignePrecedente = nouvelleConsigne; 
      } else {
        Serial.println("    >>> ❌ [ERREUR] Impossible de parler au régulateur.");
      }
    }
  }

  // ---------------------------------------------------------
  // ⬆️ LA REMONTÉE (Acquisition depuis le TOR)
  // ---------------------------------------------------------
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= INTERVALLE_SCRUTATION) {
    previousMillis = currentMillis;

    // Température
    long pv = ModbusRTUClient.holdingRegisterRead(KM3_ADDRESS, 0x0001);
    if (pv != -1) {
      modbusTCPServer.holdingRegisterWrite(0, pv); 
    }

    // Etat
    modbusTCPServer.holdingRegisterWrite(1, 2); 

    // Consigne
    long sp1 = ModbusRTUClient.holdingRegisterRead(KM3_ADDRESS, 0x0006);
    if (sp1 != -1) {
       // On ne l'écrase dans la mémoire TCP que si c'est la toute première lecture
       if (consignePrecedente == -1) {
         modbusTCPServer.holdingRegisterWrite(2, sp1); 
         consignePrecedente = sp1; 
       }
    }

    Serial.print("📊 [MONITEUR] Temp: ");
    Serial.print(pv != -1 ? String(pv/10.0) : "ERR");
    Serial.print("°C | Consigne: ");
    Serial.println(consignePrecedente != -1 ? String(consignePrecedente/10.0) : "ERR");
  }
}