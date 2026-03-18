# 🏭 Simulateur d'Autoclave Industriel - Raynal & Roquelaure

![Build Status](https://img.shields.io/badge/Build-Success-success)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-cyan)
![Protocol](https://img.shields.io/badge/Protocol-Modbus_TCP-orange)

Ce projet a été réalisé dans le cadre d'un projet de BTS pour l'entreprise **Raynal & Roquelaure**. 
Il consiste à simuler le comportement thermique d'un autoclave de stérilisation industrielle via une carte **WT32-ETH01** (ESP32 avec port Ethernet natif).

Le système intègre un serveur **Modbus TCP** permettant de communiquer avec un logiciel de supervision (SCADA) et une interface graphique locale sur un écran **TFT SPI (240x240)**.

---

## 🚀 Fonctionnalités Principales

* **Serveur Modbus TCP :** Communication réseau ultra-rapide avec des équipements industriels ou logiciels (ex: qModMaster).
* **Régulation Thermique T.O.R (Tout Ou Rien) :** Simulation réaliste de chauffe et refroidissement naturel avec une **hystérésis de ±2°C**.
* **Machine à États Dynamique :** * `0` : REPOS
  * `1` : CHAUFFE / AJUSTEMENT
  * `2` : STÉRILISATION
  * `3` : REFROIDISSEMENT
* **Sécurité Logicielle :** Bridage de la consigne maximum à **110.0 °C** (norme de stérilisation pour les conserves R&R).
* **Supervision Locale (IHM) :** Affichage en temps réel de l'état, de la température, de la consigne et de l'état du relais de chauffe sur un écran couleur TFT.

---

## 🛠️ Matériel Requis

* Carte **WT32-ETH01** (ESP32)
* Écran **TFT SPI** (Contrôleur ST7789 - Résolution 240x240)
* Câblage SPI personnalisé (MISO, MOSI, SCLK, CS, DC, RST) configuré via `TFT_eSPI`.

---

## 🔌 Table d'Échange Modbus (Mapping)

L'ESP32 agit en tant que serveur Modbus TCP (Esclave). Il écoute sur le port **502**. 
L'adresse IP statique configurée par défaut est `192.168.10.50` (Masque `255.255.255.0`).

| Registre (Décimal) | Registre (Hexa) | Type d'accès | Description | Format des données |
| :---: | :---: | :---: | :--- | :--- |
| **0** (ou 1 dans qModMaster) | `0x00` | Lecture Seule (Read) | **Température Actuelle** | Entier (Valeur x 10) |
| **1** (ou 2 dans qModMaster) | `0x01` | Lecture Seule (Read) | **État du Cycle** | `0` (Repos) à `3` (Refroidissement) |
| **2** (ou 3 dans qModMaster) | `0x02` | Lecture / Écriture (R/W) | **Consigne Demandée** | Entier (Valeur x 10) |

*(Exemple : Une valeur de `1100` lue ou écrite correspond à `110.0 °C`)*

---

## 💻 Logiciels et Dépendances

Ce projet est développé sous **VSCode** avec l'extension **PlatformIO**.

Bibliothèques utilisées (définies dans `platformio.ini`) :
* `emelianov/modbus-esp8266@^4.1.0` (Gestion de la pile Modbus TCP)
* `bodmer/TFT_eSPI@^2.5.43` (Gestion optimisée de l'affichage graphique)

---

## ⚙️ Utilisation (Test avec qModMaster)

1. Connecter la carte WT32-ETH01 au réseau local via un câble RJ45.
2. Ouvrir **qModMaster** (ou tout autre client Modbus TCP).
3. Se connecter à l'IP `192.168.10.50`, Port `502`, Unit ID `1`.
4. Utiliser la fonction `Read Holding Registers (0x03)` en démarrant à l'adresse **1** pour lire les 3 valeurs en continu.
5. Utiliser la commande `Write Single Register (0x06)` à l'adresse **3** pour envoyer une nouvelle consigne de température à la machine.

---
*Projet développé pour Raynal & Roquelaure.*