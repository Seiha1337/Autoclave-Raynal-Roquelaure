# Guide Modbus RTU - Informations complémentaires

## Comprendre Modbus RTU

### Qu'est-ce que Modbus RTU ?

Modbus RTU (Remote Terminal Unit) est un protocole de communication série largement utilisé dans l'industrie pour connecter des équipements électroniques. Il permet à un appareil maître (Master) de communiquer avec un ou plusieurs appareils esclaves (Slaves) sur un même bus.

### Architecture Maître-Esclave

```
                    ┌──────────────┐
                    │    MAÎTRE    │
                    │   (Master)   │
                    └──────┬───────┘
                           │
            ┌──────────────┴──────────────┐
            │        Bus RS485            │
            │                             │
    ┌───────┴────┐    ┌────────┴────┐   ┌┴──────────┐
    │  ESCLAVE 1 │    │  ESCLAVE 2  │   │ ESCLAVE N │
    │ (Addr: 1)  │    │ (Addr: 2)   │   │ (Addr: N) │
    └────────────┘    └─────────────┘   └───────────┘
```

### Types de registres Modbus

| Type | Nom | Accès | Taille | Adresse | Usage |
|------|-----|-------|--------|---------|-------|
| Coil | Bobine | R/W | 1 bit | 00001-09999 | Sorties digitales |
| Discrete Input | Entrée discrète | R | 1 bit | 10001-19999 | Entrées digitales |
| Input Register | Registre d'entrée | R | 16 bits | 30001-39999 | Entrées analogiques |
| Holding Register | Registre de maintien | R/W | 16 bits | 40001-49999 | Configuration, valeurs |

**Dans notre projet**, nous utilisons les **Holding Registers** (lecture/écriture).

### Fonctions Modbus utilisées

| Code | Fonction | Description | Usage dans le projet |
|------|----------|-------------|---------------------|
| 0x03 | Read Holding Registers | Lecture de registres | `mb.readHreg()` |
| 0x06 | Write Single Register | Écriture d'un registre | `mb.writeHreg()` |
| 0x10 | Write Multiple Registers | Écriture de plusieurs registres | `mb.writeHreg()` avec plusieurs valeurs |

## Format de la trame Modbus RTU

### Trame de requête (Master → Slave)

```
┌─────────┬──────────┬───────────┬──────┬─────────┐
│ Adresse │ Fonction │  Données  │ CRC  │         │
│ Esclave │   Code   │           │ 16b  │         │
│  (1B)   │   (1B)   │  (N × B)  │ (2B) │         │
└─────────┴──────────┴───────────┴──────┴─────────┘
```

### Trame de réponse (Slave → Master)

```
┌─────────┬──────────┬───────────┬──────┬─────────┐
│ Adresse │ Fonction │  Données  │ CRC  │         │
│ Esclave │   Code   │  Réponse  │ 16b  │         │
│  (1B)   │   (1B)   │  (N × B)  │ (2B) │         │
└─────────┴──────────┴───────────┴──────┴─────────┘
```

### Exemple concret

**Lecture de 10 registres à partir de l'adresse 0 depuis l'esclave 1** :

Requête Master → Slave :
```
01 03 00 00 00 0A C5 CD
│  │  │  │  │  │  └───┴─ CRC16
│  │  │  │  └──┴─────── Nombre de registres : 10
│  │  └──┴──────────── Adresse de départ : 0
│  └──────────────────── Fonction 03 (Read Holding Registers)
└────────────────────── Adresse esclave : 1
```

Réponse Slave → Master :
```
01 03 14 00 00 00 64 00 C8 ... XX XX
│  │  │  └───┴───┴───┴──┴──┴─── Valeurs des registres
│  │  └───────────────────────── Nombre d'octets de données : 20 (10 reg × 2 octets)
│  └─────────────────────────── Fonction 03
└───────────────────────────── Adresse esclave : 1
```

## Exemples de code avancés

### 1. Esclave avec différents types de registres

```cpp
#include <Arduino.h>
#include <ModbusRTU.h>

ModbusRTU mb;

// Registres Holding (40001-40010)
uint16_t holdingRegs[10];

// Coils (00001-00010)  
bool coils[10];

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 5, 17);
  
  mb.begin(&Serial2, 4);
  mb.slave(1);
  
  // Ajouter les Holding Registers
  for (int i = 0; i < 10; i++) {
    mb.addHreg(i, 0);
  }
  
  // Ajouter les Coils
  for (int i = 0; i < 10; i++) {
    mb.addCoil(i, false);
  }
}

void loop() {
  mb.task();
  
  // Lecture des Coils
  for (int i = 0; i < 10; i++) {
    coils[i] = mb.Coil(i);
  }
  
  // Mise à jour des registres
  mb.Hreg(0, analogRead(A0));  // Lecture d'un capteur
  
  delay(10);
}
```

### 2. Maître avec gestion d'erreurs avancée

```cpp
#include <Arduino.h>
#include <ModbusRTU.h>

ModbusRTU mb;
int errorCount = 0;
int successCount = 0;

bool cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  switch(event) {
    case Modbus::EX_SUCCESS:
      Serial.println("✓ Lecture réussie");
      successCount++;
      // Traitement des données
      break;
      
    case Modbus::EX_ILLEGAL_ADDRESS:
      Serial.println("✗ Erreur: Adresse invalide");
      errorCount++;
      break;
      
    case Modbus::EX_ILLEGAL_VALUE:
      Serial.println("✗ Erreur: Valeur invalide");
      errorCount++;
      break;
      
    case Modbus::EX_SLAVE_FAILURE:
      Serial.println("✗ Erreur: Échec esclave");
      errorCount++;
      break;
      
    case Modbus::EX_TIMEOUT:
      Serial.println("✗ Erreur: Timeout");
      errorCount++;
      break;
      
    default:
      Serial.printf("✗ Erreur: Code 0x%02X\n", event);
      errorCount++;
      break;
  }
  
  float successRate = (float)successCount / (successCount + errorCount) * 100;
  Serial.printf("Taux de réussite: %.1f%% (%d/%d)\n", 
                successRate, successCount, successCount + errorCount);
  
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 5, 17);
  mb.begin(&Serial2, 4);
  mb.master();
}

void loop() {
  mb.task();
  
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 1000) {
    lastRead = millis();
    mb.readHreg(1, 0, 10, cbRead);
  }
}
```

### 3. Multicast - Interroger plusieurs esclaves

```cpp
void loop() {
  mb.task();
  
  static unsigned long lastPoll = 0;
  static uint8_t currentSlave = 1;
  
  if (millis() - lastPoll > 500) {
    lastPoll = millis();
    
    Serial.printf("Interrogation esclave %d\n", currentSlave);
    mb.readHreg(currentSlave, 0, 5, cbRead);
    
    currentSlave++;
    if (currentSlave > 3) currentSlave = 1;  // 3 esclaves
  }
}
```

### 4. Esclave simulant un capteur de température/humidité

```cpp
void loop() {
  mb.task();
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    
    // Simulation d'un capteur de température (-20°C à +50°C)
    float temperature = 20.0 + random(-100, 300) / 10.0;
    int16_t tempValue = (int16_t)(temperature * 10);  // Multiplié par 10
    mb.Hreg(0, tempValue);
    
    // Simulation d'un capteur d'humidité (0-100%)
    uint16_t humidity = random(30, 80);
    mb.Hreg(1, humidity);
    
    // État du système (bit field)
    uint16_t status = 0;
    status |= (1 << 0);  // Bit 0: Système OK
    status |= (tempValue > 300) << 1;  // Bit 1: Alarme haute température
    status |= (humidity > 70) << 2;     // Bit 2: Alarme haute humidité
    mb.Hreg(2, status);
    
    Serial.printf("Temp: %.1f°C, Humidity: %d%%, Status: 0x%04X\n", 
                  temperature, humidity, status);
  }
}
```

## Optimisation des performances

### 1. Temps de réponse

- **Réduire les délais** : Minimiser les `delay()` dans la boucle
- **Fréquence de polling** : Ajuster selon les besoins (ne pas sur-interroger)
- **Timeout** : Configurer un timeout approprié

```cpp
mb.setTimeout(500);  // 500ms de timeout
```

### 2. Gestion de la bande passante

**Calcul du temps de transmission** :

```
Temps = (1 + 8 + 1 + 1) × (taille_trame_octets) / vitesse
      = 11 × taille_trame / 9600 ≈ 1.15 ms/octet
```

Pour une trame de 10 octets à 9600 bauds : ~11.5 ms

### 3. Augmenter la vitesse

Dans `platformio.ini`, vous pouvez tester différentes vitesses :

```cpp
Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);  // 19200 bauds
Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2);  // 38400 bauds
Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // 115200 bauds
```

**Attention** : Plus la vitesse est élevée, plus les erreurs augmentent sur de longues distances.

## Diagnostic et débug

### Afficher les trames Modbus

```cpp
// Dans setup()
mb.setCallback(
  [](uint8_t fc, uint16_t address, uint16_t length) {
    Serial.printf("Request: FC=0x%02X, Addr=%d, Len=%d\n", fc, address, length);
  }
);
```

### Monitorer les statistiques

```cpp
static unsigned long lastStats = 0;
if (millis() - lastStats > 10000) {  // Toutes les 10s
  lastStats = millis();
  Serial.println("=== Statistiques Modbus ===");
  Serial.printf("Requêtes réussies: %d\n", successCount);
  Serial.printf("Erreurs: %d\n", errorCount);
  Serial.printf("Uptime: %lu s\n", millis() / 1000);
}
```

## Ressources supplémentaires

- [Modbus Organization](https://modbus.org/)
- [Spécification Modbus RTU (PDF)](https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)
- [GitHub - modbus-esp8266](https://github.com/emelianov/modbus-esp8266)
- [Calculateur CRC Modbus en ligne](https://crccalc.com/)

## Projets d'extension

1. **Ajout d'un écran OLED** : Afficher les valeurs Modbus sur un écran
2. **Intégration Web** : Créer un serveur Web pour visualiser les données
3. **Logger de données** : Enregistrer les échanges sur carte SD
4. **Passerelle Modbus-MQTT** : Convertir Modbus vers MQTT pour IoT
5. **Analyseur de bus** : Créer un sniffer Modbus pour débugger
