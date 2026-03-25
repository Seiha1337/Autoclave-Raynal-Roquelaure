// ╔══════════════════════════════════════════════════════════╗
// ║      SIMULATEUR AUTOCLAVE - RAYNAL & ROQUELAURE          ║
// ║      - Modbus TCP Server pour PC SCADA (Port 502)        ║
// ║      - UI TFT Modernisée                                 ║
// ╚══════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include <ETH.h>
#include <ModbusIP_ESP8266.h>
#include <TFT_eSPI.h>

// --- Configuration Spécifique de la puce WT32-ETH01 ---
#define ETH_PHY_ADDR  1
#define ETH_PHY_POWER 16
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

// --- Paramètres Réseau (Autoclave 2 = .51) ---
IPAddress local_ip(192, 168, 50, 51); 
IPAddress gateway(192, 168, 50, 1);
IPAddress subnet(255, 255, 255, 0);

// --- Objets ---
ModbusIP mb;
TFT_eSPI tft = TFT_eSPI();

// --- Registres Modbus ---
const int REG_TEMP     = 0x00; 
const int REG_STATE    = 0x01;  
const int REG_CONSIGNE = 0x02; 

// --- Variables Simulation Autoclave ---
float temperature = 20.0;
float consigne = 110.0; 
int cycleState = 0; 
int timerSterilisation = 0;
unsigned long lastUpdate = 0;

// Variables de Régulation TOR
bool relaisChauffe = false; 
const float HYSTERESIS = 2.0;

// Variables anti-scintillement écran
int lastState = -1; 
float lastConsigne = -1.0;
bool lastRelais = !relaisChauffe;

// --- Palette de couleurs "Dark UI" ---
#define COLOR_BG       tft.color565(15, 15, 20)   // Fond très sombre
#define COLOR_CARD     tft.color565(35, 35, 45)   // Gris bleuté pour les encarts
#define COLOR_RR_RED   tft.color565(220, 30, 30)  // Rouge entreprise
#define COLOR_TEXT_DIM tft.color565(150, 150, 150)// Texte secondaire

void drawStaticUI() {
  tft.fillScreen(COLOR_BG);
  
  // Header principal
  tft.fillRoundRect(5, 5, 230, 40, 6, COLOR_RR_RED);
  tft.setTextColor(TFT_WHITE, COLOR_RR_RED);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("RAYNAL & ROQUELAURE", 120, 18, 2);
  tft.setTextColor(TFT_YELLOW, COLOR_RR_RED);
  tft.drawString("AUTOCLAVE 2", 120, 33, 1);

  // Carte 1 : Processus (Température & Consigne)
  tft.fillRoundRect(5, 55, 230, 115, 8, COLOR_CARD);
  tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD);
  tft.setTextDatum(TL_DATUM); 
  tft.drawString("TEMPERATURE C.", 15, 65, 2);
  tft.drawString("CONSIGNE SV.", 15, 145, 2);

  // Carte 2 : Système (Statut & Relais)
  tft.fillRoundRect(5, 180, 230, 55, 8, COLOR_CARD);
  tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD);
  tft.setTextDatum(TL_DATUM); 
  tft.drawString("Vanne Vapeur :", 15, 190, 2);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init Ecran
  tft.init();
  tft.setRotation(0); 
  drawStaticUI();

  // Init Réseau
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
  ETH.config(local_ip, gateway, subnet);

  // Init Modbus
  mb.server();
  mb.addHreg(REG_TEMP, 200); 
  mb.addHreg(REG_STATE, 0); 
  mb.addHreg(REG_CONSIGNE, 1100); 
}

void loop() {
  mb.task();

  // 1. LECTURE DES ORDRES DU MAITRE (PC SCADA)
  uint16_t consigneModbus = mb.Hreg(REG_CONSIGNE);
  if (consigneModbus > 1100) { consigneModbus = 1100; mb.Hreg(REG_CONSIGNE, 1100); }
  consigne = consigneModbus / 10.0;

  uint16_t ordreMaitre = mb.Hreg(REG_STATE);

  // Si le SCADA ordonne l'arrêt d'urgence
  if (ordreMaitre == 0 && cycleState != 0 && cycleState != 3) {
    cycleState = 3; 
  }

  // 2. MOTEUR DE SIMULATION (Toutes les 1s)
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();

    switch (cycleState) {
      case 0: // REPOS
        temperature = 20.0; 
        relaisChauffe = false;
        if (ordreMaitre == 1) { cycleState = 1; }
        break;

      case 1: // CHAUFFE OU AJUSTEMENT
        if (temperature < (consigne - HYSTERESIS)) {
          relaisChauffe = true;
          temperature += 2.5; 
        } 
        else if (temperature > (consigne + HYSTERESIS)) {
          relaisChauffe = false;
          temperature -= 1.5; 
        } 
        else {
          relaisChauffe = false;
          cycleState = 2; 
          timerSterilisation = 0; 
        }
        break;

      case 2: // STERILISATION
        if (abs(temperature - consigne) > (HYSTERESIS + 2.0)) {
          cycleState = 1; 
        } else {
          if (temperature <= (consigne - HYSTERESIS)) relaisChauffe = true;
          if (temperature >= (consigne + HYSTERESIS)) relaisChauffe = false;

          if (relaisChauffe) temperature += 0.8; 
          else temperature -= 0.6;              

          timerSterilisation++; 
          if (timerSterilisation > 60) {
            cycleState = 3;
          }
        }
        break;

      case 3: // REFROIDISSEMENT FINAL
        relaisChauffe = false;
        temperature -= 3.0; 
        if (temperature <= 20.0) {
          cycleState = 0; 
        }
        break;
    }

    // --- SYNCHRONISATION MODBUS ---
    mb.Hreg(REG_TEMP, (uint16_t)(temperature * 10));
    mb.Hreg(REG_STATE, cycleState); // Correction vitale pour le suivi du PC

    // --- RAFRAICHISSEMENT ECRAN TFT ---
    
    // Affichage de l'état du cycle (en bas de la carte 2)
    if (cycleState != lastState) {
      tft.setTextDatum(MC_DATUM);
      tft.fillRoundRect(15, 210, 210, 20, 4, COLOR_BG); // Efface l'ancien texte
      
      if (cycleState == 0) { tft.setTextColor(TFT_DARKGREY, COLOR_BG); tft.drawString("MACHINE EN REPOS", 120, 220, 2); }
      if (cycleState == 1) { 
        if (temperature < consigne) {
          tft.setTextColor(TFT_ORANGE, COLOR_BG); tft.drawString("MONTEE EN TEMPERATURE", 120, 220, 2); 
        } else {
          tft.setTextColor(TFT_ORANGE, COLOR_BG); tft.drawString("AJUSTEMENT THERMIQUE", 120, 220, 2); 
        }
      }
      if (cycleState == 2) { tft.setTextColor(TFT_GREEN, COLOR_BG); tft.drawString("STERILISATION EN COURS", 120, 220, 2); }
      if (cycleState == 3) { tft.setTextColor(TFT_CYAN, COLOR_BG); tft.drawString("REFROIDISSEMENT...", 120, 220, 2); }
      lastState = cycleState;
    }

    // Affichage des valeurs numériques (Carte 1)
    tft.setTextDatum(MC_DATUM); 
    tft.setTextColor(TFT_WHITE, COLOR_CARD);
    
    // Redessine uniquement la zone du chiffre pour éviter les clignotements
    tft.fillRect(40, 85, 160, 45, COLOR_CARD);
    char tempStr[10];
    sprintf(tempStr, "%.1f C", temperature);
    tft.drawString(tempStr, 120, 110, 6); 

    if (consigne != lastConsigne) {
      tft.setTextDatum(MR_DATUM);
      tft.fillRect(130, 140, 100, 25, COLOR_CARD); 
      tft.setTextColor(TFT_CYAN, COLOR_CARD);
      char consStr[10];
      sprintf(consStr, "%.1f C", consigne);
      tft.drawString(consStr, 225, 152, 4); 
      lastConsigne = consigne;
      lastState = -1; // Force le rafraîchissement du statut
    }

    // Affichage du Relais (Carte 2)
    if (relaisChauffe != lastRelais) {
      tft.setTextDatum(ML_DATUM);
      tft.fillRect(150, 185, 70, 20, COLOR_CARD);
      if (relaisChauffe) {
        tft.fillRoundRect(150, 185, 60, 20, 4, TFT_GREEN);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.drawString(" ON ", 165, 195, 2);
      } else {
        tft.fillRoundRect(150, 185, 60, 20, 4, COLOR_BG);
        tft.setTextColor(TFT_WHITE, COLOR_BG);
        tft.drawString(" OFF", 165, 195, 2);
      }
      lastRelais = relaisChauffe;
    }
  }
}