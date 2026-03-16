# 🤖 Automate Finder OPTA - Modbus TCP (Esclave)

![Platform](https://img.shields.io/badge/Platform-Arduino_Opta-green)
![Protocol](https://img.shields.io/badge/Protocol-Modbus_TCP-orange)
![Role](https://img.shields.io/badge/Role-Esclave_Serveur-blue)

## 📌 Rôle dans le projet Raynal & Roquelaure
Ce dossier contient le programme de l'automate industriel **Finder OPTA**. 
Dans l'architecture de notre supervision d'autoclave, cet automate est configuré en tant que **Serveur (Esclave) Modbus TCP**. Il simule l'acquisition des données matérielles (comme la température de la cuve) et les met à disposition sur le réseau industriel pour être lues par un logiciel de supervision (ex: qModMaster ou notre IHM Web).

## ⚙️ Configuration Réseau
L'automate est configuré avec une adresse IP statique pour garantir une connexion stable sur le réseau de l'usine :
* **Adresse MAC :** `A8:61:0A:AE:76:05`
* **Adresse IP (Fixe) :** `192.168.50.210`
* **Port Modbus :** `502`

## 🔌 Adressage Modbus (Holding Registers)
L'automate expose 3 registres principaux à partir de l'adresse `0x00` :

| Registre | Donnée | Accès | Description |
| :---: | :--- | :---: | :--- |
| **0** | Température | R | Simulation d'une sonde PT100 (Valeur x10). Ex: `265` = 26.5 °C. S'incrémente automatiquement pour les tests. |
| **1** | État | R | État du cycle de l'autoclave. |
| **2** | Consigne | R/W | Consigne de chauffe (Bridée à 110.0 °C). |

## 🛠️ Prérequis Logiciels
Pour téléverser ou modifier ce code, vous devez utiliser :
1. **Arduino IDE 2.x** (Obligatoire pour la gestion correcte des cœurs `mbed_opta`).
2. Le package de cartes **Arduino mbed OS Opta Boards**.
3. La bibliothèque officielle **`ArduinoModbus`** (qui inclut `ArduinoRS485`).

*Note : Si le port COM n'est pas détecté par Windows, effectuez un double-clic rapide sur le bouton `RESET` de l'automate pour forcer le mode Bootloader.*