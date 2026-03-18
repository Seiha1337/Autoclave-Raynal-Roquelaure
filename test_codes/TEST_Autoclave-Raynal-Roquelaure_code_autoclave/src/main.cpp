// ╔══════════════════════════════════════════════════════════╗
// ║      SIMULATEUR AUTOCLAVE - RAYNAL & ROQUELAURE          ║
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

// --- Paramètres Réseau Fixes ---
IPAddress local_ip(192, 168, 10, 50); 
IPAddress gateway(192, 168, 10, 1);
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
const float HYSTERESIS = 2.0; // Marge de +- 2°C

// Variables anti-scintillement écran
int lastState = -1; 
float lastConsigne = -1.0;
bool lastRelais = !relaisChauffe;

// --- Couleurs Personnalisées ---
#define COLOR_RR_RED   tft.color565(200, 20, 20)
#define COLOR_DARKBOX  tft.color565(30, 30, 30)

// --- Fonction pour dessiner l'interface statique ---
void drawStaticUI() {
  tft.fillScreen(TFT_BLACK);
  
  // 1. En-tête Raynal & Roquelaure
  tft.fillRoundRect(5, 5, 230, 40, 5, COLOR_RR_RED);
  tft.setTextColor(TFT_WHITE, COLOR_RR_RED);
  tft.setTextDatum(MC_DATUM); 
  tft.drawString("RAYNAL & ROQUELAURE", 120, 18, 2);
  tft.setTextColor(TFT_YELLOW, COLOR_RR_RED);
  tft.drawString("AUTOCLAVE N°1", 120, 33, 1);

  // 2. Cadres pour les données
  tft.drawRoundRect(5, 90, 230, 70, 5, TFT_DARKGREY); // Cadre Température
  tft.drawRoundRect(5, 165, 230, 45, 5, TFT_DARKGREY); // Cadre Consigne

  // Labels
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); 
  tft.drawString("Temperature :", 15, 95, 2);
  tft.drawString("Consigne :", 15, 170, 2);
  
  // Label Relais
  tft.drawString("Relais Chauffe :", 15, 220, 2);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Initialisation de l'écran TFT
  tft.init();
  tft.setRotation(0); 
  drawStaticUI();

  // 2. Allumage de la puce réseau
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
  ETH.config(local_ip, gateway, subnet);

  // 3. Démarrage Modbus TCP
  mb.server();
  mb.addHreg(REG_TEMP, 200); 
  mb.addHreg(REG_STATE, 0);  
  mb.addHreg(REG_CONSIGNE, 1100); 
}

void loop() {
  mb.task();

  // Lecture Consigne (Sécurité 110°C)
  uint16_t consigneModbus = mb.Hreg(REG_CONSIGNE);
  if (consigneModbus > 1100) {
    consigneModbus = 1100;
    mb.Hreg(REG_CONSIGNE, 1100); 
  }
  consigne = consigneModbus / 10.0;

  // Mise à jour de la simulation (Toutes les 1s)
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();

    // --- REGULATION T.O.R à +-2°C ---
    switch (cycleState) {
      case 0: // REPOS
        temperature = 20.0; 
        relaisChauffe = true;
        cycleState = 1; 
        break;

      case 1: // CHAUFFE OU AJUSTEMENT
        if (temperature < (consigne - HYSTERESIS)) {
          relaisChauffe = true;
          temperature += 2.5; // Chauffe forte
        } 
        else if (temperature > (consigne + HYSTERESIS)) {
          relaisChauffe = false;
          temperature -= 1.5; // Refroidissement actif pour redescendre à la consigne !
        } 
        else {
          // La température est enfin dans la bonne zone de la consigne (+- 2°C)
          relaisChauffe = false;
          cycleState = 2; // On passe en stérilisation
          timerSterilisation = 0; 
        }
        break;

      case 2: // STERILISATION (Maintien TOR)
        // Vérification si la consigne a été baissée ou augmentée brutalement
        if (abs(temperature - consigne) > (HYSTERESIS + 2.0)) {
          cycleState = 1; // Ecart trop grand, on retourne s'ajuster en Etat 1
        } else {
          // Logique d'oscillation +- 2°C
          if (temperature <= (consigne - HYSTERESIS)) relaisChauffe = true;
          if (temperature >= (consigne + HYSTERESIS)) relaisChauffe = false;

          // Application physique
          if (relaisChauffe) temperature += 0.8; // La chauffe est rapide
          else temperature -= 0.6;               // Le refroidissement naturel est un peu plus lent

          timerSterilisation++; 
          if (timerSterilisation > 60) cycleState = 3; // Cycle de 60 secondes pour bien voir l'effet
        }
        break;

      case 3: // REFROIDISSEMENT FINAL
        relaisChauffe = false;
        temperature -= 3.0; 
        if (temperature <= 20.0) cycleState = 0; 
        break;
    }

    // Écriture Modbus
    mb.Hreg(REG_TEMP, (uint16_t)(temperature * 10));
    mb.Hreg(REG_STATE, cycleState);

    // --- MISE A JOUR DE L'ECRAN ---
    
    // 1. Bandeau d'état
    if (cycleState != lastState) {
      tft.setTextDatum(MC_DATUM);
      tft.fillRect(5, 55, 230, 25, TFT_BLACK); 
      
      if (cycleState == 0) { tft.setTextColor(TFT_BLUE, TFT_BLACK); tft.drawString(">>> REPOS <<<", 120, 67, 4); }
      if (cycleState == 1) { 
        if (temperature < consigne) {
          tft.setTextColor(TFT_RED, TFT_BLACK); tft.drawString(">>> CHAUFFE <<<", 120, 67, 4); 
        } else {
          tft.setTextColor(TFT_ORANGE, TFT_BLACK); tft.drawString(">>> AJUSTEMENT <<<", 120, 67, 4); 
        }
      }
      if (cycleState == 2) { tft.setTextColor(TFT_MAGENTA, TFT_BLACK); tft.drawString(">>> STERILISATION <<<", 120, 67, 4); }
      if (cycleState == 3) { tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.drawString(">>> REFROIDISSEMENT <<<", 120, 67, 2); }
      lastState = cycleState;
    }

    // 2. Température (Centrée dans son cadre)
    tft.setTextDatum(MC_DATUM); 
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.fillRect(10, 115, 220, 40, TFT_BLACK); // Nettoie la zone
    char tempStr[10];
    sprintf(tempStr, "%.1f C", temperature);
    tft.drawString(tempStr, 120, 135, 6); 

    // 3. Consigne
    if (consigne != lastConsigne) {
      tft.setTextDatum(MR_DATUM);
      tft.fillRect(130, 175, 100, 30, TFT_BLACK); 
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      char consStr[10];
      sprintf(consStr, "%.1f C", consigne);
      tft.drawString(consStr, 225, 190, 4); 
      lastConsigne = consigne;
      lastState = -1; // Force l'écran à rafraîchir le texte Chauffe/Ajustement
    }

    // 4. Indicateur Visuel du Relais
    if (relaisChauffe != lastRelais) {
      tft.setTextDatum(ML_DATUM);
      tft.fillRect(150, 215, 80, 20, TFT_BLACK);
      if (relaisChauffe) {
        tft.fillRoundRect(150, 215, 60, 20, 3, TFT_GREEN);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.drawString(" ON ", 165, 225, 2);
      } else {
        tft.fillRoundRect(150, 215, 60, 20, 3, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.drawString(" OFF", 165, 225, 2);
      }
      lastRelais = relaisChauffe;
    }
  }
}