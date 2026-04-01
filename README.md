# 🏭 Projet SCADA & Automatisme - Raynal & Roquelaure
![CI - Vérification Firmware ESP32](https://github.com/Seiha1337/Projet-Raynal-Roquelaure/actions/workflows/build_esp32.yml/badge.svg)
[![Build and Publish Docker Image](https://github.com/Seiha1337/projet_raynal-roquelaure/actions/workflows/docker-publish.yml/badge.svg)](https://github.com/Seiha1337/projet_raynal-roquelaure/actions/workflows/docker-publish.yml)


## 📖 Contexte du Projet
Projet de fin d'études (BTS) réalisé pour l'entreprise **Raynal & Roquelaure**. 
L'objectif de cette infrastructure est de moderniser, superviser et automatiser un parc de 6 autoclaves industriels via un réseau **Modbus TCP/IP** et d'assurer la traçabilité complète des cycles de stérilisation.

---

## 🏗️ Architecture du Système
Le réseau est structuré selon un modèle industriel Client/Serveur (Maître/Esclaves) :

* **🖥️ Le Cerveau (Supervision SCADA) :** Un PC exécutant une API métier (**Node.js**) chargée du polling réseau, de l'historisation des températures et de la tenue d'un journal des incidents dans une base de données **MariaDB/MySQL**.
* **⚙️ Autoclave 1 (Master Matériel) :** Automate programmable **Finder OPTA** pilotant les relais industriels (Vannes Vapeur/Eau) et communiquant avec un régulateur PID **Ascon KM3** via une passerelle **Modbus RTU (RS485)**.
* **📟 Autoclaves 2 à 6 (Simulateurs) :** Cartes microcontrôleurs **ESP32 (WT32-ETH01)** simulant des cycles de stérilisation complets (Régulation T.O.R avec hystérésis) et disposant d'une IHM locale sur écran TFT.

---

## 📂 Arborescence du Dépôt

Le projet est divisé en plusieurs modules distincts, séparant la logique matérielle, logicielle et documentaire :

### 1. Cœur du Système (Nouvelle Architecture)
* 📁 **`Firmware/API_Supervision_Master/`** : Daemon d'acquisition Node.js (Modbus TCP) et scripts de connexion à la base de données.
* 📁 **`Firmware/ESP32_Slaves_Autoclaves/`** : Code source C++ (PlatformIO) des simulateurs WT32-ETH01 avec serveur Modbus TCP intégré et affichage TFT.
* 📁 **`Firmware/OPTA_Slave_Autoclave/`** : Code C++ de l'automate industriel Finder OPTA.

### 2. Interface et Base de Données
* 📁 **`APPLICATION/`** : Code source de l'interface web (IHM) destinée au contrôle à distance par les opérateurs.
* 📁 **`Base_De_Donnees/`** *(à venir)* : Modèles et scripts SQL pour la création des tables (`mesures_autoclaves`, `journal_incidents`).

### 3. Tests & Maquettage
* 📁 **`testModbusRtuRS485_WT32/`** : Scripts et essais de communication série RS485 pour l'interfaçage avec le régulateur PID Ascon KM3.
* 📁 **`Autoclave-Raynal-Roquelaure/`** & 📁 **`Autoclave CODE ESP32/`** : Archives, anciennes versions et codes de secours.

### 4. Ingénierie & Documentation
* 📁 **`DOCUMENTATION/`** : Cahier des charges, schémas de câblage, datasheets des composants, plan d'adressage IP et rapports d'audit réseau.

---

## 🚀 État d'Avancement (Tâches E6)

- [x] Définition du plan d'adressage IP et cartographie du réseau.
- [x] Développement C++ des automates virtuels (ESP32) avec serveur Modbus TCP.
- [x] Interface Graphique (TFT) dynamique des cycles de stérilisation.
- [x] Script d'acquisition et de polling réseau (Node.js).
- [ ] Création et liaison de la base de données (Historisation & Journal des incidents).
- [ ] Communication Modbus RTU (RS485) avec le régulateur Ascon KM3.
- [ ] Finalisation de l'IHM Web globale.

---

## 🛠️ Technologies & Outils
* **Matériel :** Automate Finder OPTA, WT32-ETH01 (ESP32), Régulateur Ascon KM3, Écrans TFT SPI.
* **Logiciel :** VS Code, PlatformIO, Node.js (`modbus-serial`), XAMPP (MariaDB).
* **Protocoles :** Modbus TCP/IP (Port 502), Modbus RTU (RS485), Ethernet.

---
*Projet réalisé dans le cadre de l'épreuve E6 - Conception, Développement et Maintenance de Réseaux Industriels.*
