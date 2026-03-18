# Guide de démarrage rapide

## Installation et compilation

### 1. Prérequis
- [PlatformIO](https://platformio.org/) installé dans VS Code
- Deux cartes WT32-ETH01
- Deux modules RS485 (MAX485 ou similaire)

### 2. Cloner/Ouvrir le projet
```bash
cd "e:\Cours - Drive Google\BTS CIEL\BTS CIEL_2025-2026\testModbusRtuRS485_WT32"
```

### 3. Installer les dépendances
```bash
pio pkg install
```

Les bibliothèques suivantes seront installées automatiquement :
- `emelianov/modbus-esp8266@^4.1.0`

## Compilation et téléversement

### Carte 1 : ESCLAVE (par défaut)

Le fichier `main.cpp` contient déjà le code Esclave.

```bash
# Compiler
pio run

# Téléverser
pio run -t upload

# Ouvrir le moniteur série
pio device monitor -b 115200
```

### Carte 2 : MAÎTRE

**Méthode 1** : Copier manuellement le fichier
```bash
# Windows PowerShell
Copy-Item "src\main_master.cpp" -Destination "src\main.cpp" -Force

# Compiler et téléverser
pio run -t upload

# Ouvrir le moniteur série
pio device monitor -b 115200
```

**Méthode 2** : Utiliser des environnements séparés

Modifier `platformio.ini` :
```ini
[env:wt32-eth01-slave]
platform = espressif32
board = wt32-eth01
framework = arduino
lib_deps = 
    emelianov/modbus-esp8266@^4.1.0
monitor_speed = 115200
build_src_filter = +<main_slave.cpp>

[env:wt32-eth01-master]
platform = espressif32
board = wt32-eth01
framework = arduino
lib_deps = 
    emelianov/modbus-esp8266@^4.1.0
monitor_speed = 115200
build_src_filter = +<main_master.cpp>
```

Puis compiler pour un environnement spécifique :
```bash
# Compiler l'esclave
pio run -e wt32-eth01-slave

# Compiler le maître
pio run -e wt32-eth01-master

# Téléverser l'esclave sur COM3
pio run -e wt32-eth01-slave -t upload --upload-port COM3

# Téléverser le maître sur COM4
pio run -e wt32-eth01-master -t upload --upload-port COM4
```

## Commandes PlatformIO utiles

### Compilation
```bash
# Compiler le projet
pio run

# Nettoyer le projet
pio run -t clean

# Compiler en mode verbose
pio run -v
```

### Téléversement
```bash
# Téléverser sur le port par défaut
pio run -t upload

# Téléverser sur un port spécifique
pio run -t upload --upload-port COM3

# Lister les ports série disponibles
pio device list
```

### Moniteur série
```bash
# Ouvrir le moniteur série (115200 bauds)
pio device monitor -b 115200

# Avec port spécifique
pio device monitor -b 115200 -p COM3

# Fermer avec Ctrl+C
```

### Autres commandes utiles
```bash
# Lister les bibliothèques installées
pio pkg list

# Mettre à jour les bibliothèques
pio pkg update

# Rechercher une bibliothèque
pio pkg search modbus

# Informations sur la carte
pio boards wt32-eth01

# Nettoyer complètement le projet
pio run -t cleanall
```

## Tests rapides

### Test 1 : Vérification du câblage

Avant de tester Modbus, vérifiez la communication série :

**Code de test simple** (`test_serial.cpp`) :
```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 5, 17);
  
  Serial.println("Test RS485");
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);  // Mode émission
}

void loop() {
  Serial2.println("Test");
  Serial.println("Envoyé: Test");
  delay(1000);
}
```

### Test 2 : Communication Loopback

Connectez RX et TX ensemble sur le module RS485 pour tester en boucle locale.

### Test 3 : Scan des esclaves (Master)

```cpp
void scanSlaves() {
  Serial.println("Scan des esclaves Modbus...");
  
  for (uint8_t addr = 1; addr <= 10; addr++) {
    Serial.printf("Test esclave %d... ", addr);
    
    if (mb.readHreg(addr, 0, 1, cbRead)) {
      delay(100);  // Attendre la réponse
      Serial.println("Trouvé!");
    } else {
      Serial.println("Pas de réponse");
    }
    
    delay(50);
  }
}

void setup() {
  // ... configuration habituelle ...
  scanSlaves();
}
```

## Surveillance simultanée des deux cartes

### Windows (PowerShell)

Ouvrir deux terminaux PowerShell :

**Terminal 1** :
```powershell
pio device monitor -b 115200 -p COM3
```

**Terminal 2** :
```powershell
pio device monitor -b 115200 -p COM4
```

### Alternative : Utiliser PuTTY ou Tera Term

Télécharger [PuTTY](https://www.putty.org/) ou [Tera Term](https://ttssh2.osdn.jp/) pour avoir plusieurs fenêtres de monitoring.

## Résolution des problèmes courants

### Erreur : "Port COM not found"
```bash
# Lister les ports disponibles
pio device list

# Spécifier le port manuellement
pio run -t upload --upload-port COM3
```

### Erreur : "Library not found"
```bash
# Réinstaller les bibliothèques
pio pkg install -e wt32-eth01

# Ou forcer la mise à jour
pio pkg update
```

### Erreur de compilation
```bash
# Nettoyer et recompiler
pio run -t clean
pio run
```

### Problème de droits (Windows)
Exécuter PowerShell en tant qu'administrateur si nécessaire.

## Workflow de développement recommandé

1. **Développement** :
   ```bash
   # Compiler sans téléverser
   pio run
   ```

2. **Test sur une carte** :
   ```bash
   # Téléverser et monitorer
   pio run -t upload && pio device monitor -b 115200
   ```

3. **Déploiement sur les deux cartes** :
   ```bash
   # Carte 1 (Esclave) sur COM3
   pio run -t upload --upload-port COM3
   
   # Carte 2 (Maître) sur COM4
   # (Après avoir changé main.cpp)
   pio run -t upload --upload-port COM4
   ```

4. **Monitoring simultané** :
   - Ouvrir deux terminaux
   - Monitorer chaque carte séparément

## Personnalisation avancée

### Ajouter des options de compilation

Dans `platformio.ini` :
```ini
[env:wt32-eth01]
platform = espressif32
board = wt32-eth01
framework = arduino
lib_deps = 
    emelianov/modbus-esp8266@^4.1.0
monitor_speed = 115200

; Options de debug
build_flags = 
    -D DEBUG=1
    -D MODBUS_DEBUG=1
    
; Augmenter la taille de la pile
board_build.f_cpu = 240000000L
```

### Utiliser les tâches VS Code

Créer `.vscode/tasks.json` :
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "PlatformIO: Build Slave",
      "type": "shell",
      "command": "pio run -e wt32-eth01-slave"
    },
    {
      "label": "PlatformIO: Build Master",
      "type": "shell",
      "command": "pio run -e wt32-eth01-master"
    },
    {
      "label": "PlatformIO: Upload Slave",
      "type": "shell",
      "command": "pio run -e wt32-eth01-slave -t upload --upload-port COM3"
    },
    {
      "label": "PlatformIO: Upload Master",
      "type": "shell",
      "command": "pio run -e wt32-eth01-master -t upload --upload-port COM4"
    }
  ]
}
```

## Checklist de mise en service

- [ ] Câblage vérifié (A-A, B-B, GND-GND)
- [ ] Alimentation des deux cartes
- [ ] Bibliothèque Modbus installée
- [ ] Code Slave compilé et téléversé sur Carte 1
- [ ] Code Master compilé et téléversé sur Carte 2
- [ ] Les deux moniteurs série ouverts
- [ ] Communication fonctionnelle (voir les échanges)

## Support et documentation

- **README principal** : [README.md](../README.md)
- **Câblage détaillé** : [WIRING.md](WIRING.md)
- **Guide Modbus** : [MODBUS_GUIDE.md](MODBUS_GUIDE.md)
- **PlatformIO Docs** : https://docs.platformio.org/

## Contacts

Projet pédagogique - BTS CIEL 2025-2026

Pour toute question, consulter la documentation ou contacter l'enseignant.
