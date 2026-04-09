require('dotenv').config(); 
const ModbusRTU = require("modbus-serial");
const mysql = require("mysql2/promise");
const express = require('express'); // 🆕 Ajout du module Web

// ==========================================================
// 🛡️ CONFIGURATION & MÉMOIRE DES INCIDENTS
// ==========================================================
const DELAI_LOG_INCIDENT = 10 * 60 * 1000; 
const derniersIncidentsEnregistres = {}; 

// Initialisation de l'API Express
const app = express();
app.use(express.json()); // 🆕 Permet de comprendre les données envoyées par Nathan

// ==========================================================
// 🛡️ BOUCLIER ANTI-CRASH GLOBAL
// ==========================================================
process.on('uncaughtException', (err) => {
    if (err.code !== 'ECONNREFUSED' && err.code !== 'EHOSTUNREACH' && err.code !== 'ETIMEDOUT') {
        console.error("⚠️ Erreur critique interceptée :", err.message);
    }
});

process.on('unhandledRejection', (reason, promise) => {});

console.log("==========================================================");
console.log(" 🖥️ SUPERVISION INDUSTRIELLE : ACQUISITION & COMMANDE");
console.log("==========================================================");

const pool = mysql.createPool({
    host: process.env.DB_HOST,         
    port: process.env.DB_PORT,         
    user: process.env.DB_USER,         
    password: process.env.DB_PASSWORD, 
    database: process.env.DB_NAME,     
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
});

const autoclaves = [
    { id: "Autoclave 1 (OPTA)", ip: "172.40.1.50", mac: "A8:61:0A:AE:76:05" },
    { id: "Autoclave 2 (ESP32)", ip: "172.40.1.51", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 3 (ESP32)", ip: "172.40.1.52", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 4 (ESP32)", ip: "172.40.1.53", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 5 (ESP32)", ip: "172.40.1.54", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 6 (ESP32)", ip: "172.40.1.55", mac: "XX:XX:XX:XX:XX:XX" }
];

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// ==========================================================
// 📥 API D'ÉCOUTE POUR LA COMMANDE OPERATEUR (DESCENTE)
// ==========================================================
app.post('/api/consigne', async (req, res) => {
    const ipMachine = req.body.ip; 
    const nouvelleConsigne = req.body.valeur; 

    if (!ipMachine || !nouvelleConsigne) {
        return res.status(400).json({ erreur: "IP ou valeur manquante" });
    }

    console.log(`\n⚙️ [COMMANDE OPÉRATEUR] Envoi de la consigne ${nouvelleConsigne}°C vers ${ipMachine}...`);

    const clientCommande = new ModbusRTU();
    clientCommande.setTimeout(2000);

    try {
        await clientCommande.connectTCP(ipMachine, { port: 502 });
        clientCommande.setID(1); 

        // Les automates attendent souvent une valeur multipliée par 10 pour la précision
        const valeurModbus = Math.round(nouvelleConsigne * 10); 
        
        // Adresse 2 = L'adresse du registre de consigne (à adapter selon ta doc Modbus)
        await clientCommande.writeRegister(2, valeurModbus); 

        console.log(`✅ [SUCCÈS] Consigne transmise à l'automate !`);
        res.status(200).json({ message: "Consigne appliquée avec succès" });

    } catch (e) {
        console.error(`❌ [ÉCHEC] Impossible d'envoyer la commande :`, e.message);
        res.status(500).json({ erreur: "Erreur de communication avec l'autoclave" });
    } finally {
        clientCommande.close();
    }
});

// ==========================================================
// 🚨 FONCTION D'ENREGISTREMENT DU JOURNAL D'INCIDENTS
// ==========================================================
async function enregistrerIncident(machine, type, details) {
    const maintenant = Date.now();
    const derniereAlerte = derniersIncidentsEnregistres[machine.id] || 0;

    if (maintenant - derniereAlerte > DELAI_LOG_INCIDENT) {
        try {
            const query = `
                INSERT INTO journal_incidents (autoclave_id, type_incident, description) 
                VALUES (?, ?, ?)
            `;
            const numeroAutoclave = machine.id.match(/\d+/)[0];
            await pool.execute(query, [numeroAutoclave, type, details]);
            
            derniersIncidentsEnregistres[machine.id] = maintenant; 
            console.log(`    🚨 [LOGGED] Incident enregistré en BDD pour ${machine.id}`);
        } catch (err) {
            console.error("❌ Erreur enregistrement journal_incidents :", err.message);
        }
    } else {
        console.log(`    ⚠️ [FILTRÉ] Incident déjà signalé pour ${machine.id} (Rétention 10min)`);
    }
}

// ==========================================================
// 🔍 SCRUTATION D'UNE MACHINE (RE MONTÉE)
// ==========================================================
async function scruterAutoclave(machine) {
    const client = new ModbusRTU();
    client.setTimeout(2000); 

    try {
        await client.connectTCP(machine.ip, { port: 502 });
        client.setID(1);

        const data = await client.readHoldingRegisters(0, 3);
        
        const temperature = data.data[0] / 10.0;
        const etatMachine = data.data[1];
        const consigne = data.data[2] / 10.0;

        let etatTexte = "Arrêt";
        let chauffeActuelle = "OFF";
        
        if (etatMachine === 1) {
            etatTexte = "Montée en Température";
            chauffeActuelle = "ON 🔥";
        } else if (etatMachine === 2) {
            etatTexte = "Stérilisation";
            chauffeActuelle = "RÉGULATION ⚖️";
        } else if (etatMachine === 3) {
            etatTexte = "Refroidissement";
            chauffeActuelle = "OFF ❄️";
        }

        console.log(`[🟢 EN LIGNE] ${machine.id} | Temp: ${temperature}°C | Etat: ${etatTexte}`);

        try {
            const query = `
                INSERT INTO mesures_autoclaves 
                (autoclave_id, temperature, consigne, cycle, etat) 
                VALUES (?, ?, ?, ?, ?)
            `;
            const numeroAutoclave = machine.id.match(/\d+/)[0];
            await pool.execute(query, [numeroAutoclave, temperature, consigne, etatTexte, chauffeActuelle]);
            console.log(`    ↳ 💾 Mesure sauvegardée.`);
        } catch (dbError) {
            console.log(`    ↳ ⚠️ Erreur BDD Mesures: ${dbError.message}`);
        }

    } catch (e) {
        let typeErreur = "ERREUR_COMMUNICATION";
        let detailErreur = e.message;

        if (e.code === 'ETIMEDOUT' || e.message.includes('Timeout')) {
            typeErreur = "TIMEOUT_RESEAU";
            detailErreur = "L'automate ne répond pas (Timeout 2s).";
        } else if (e.code === 'ECONNREFUSED') {
            typeErreur = "CONNEXION_REFUSEE";
            detailErreur = "L'automate refuse la connexion sur le port 502.";
        } else if (e.code === 'EHOSTUNREACH') {
            typeErreur = "HOTE_INJOIGNABLE";
            detailErreur = "L'IP est injoignable sur le réseau local.";
        }

        console.log(`[🔴 HORS LIGNE] ${machine.id} - ${typeErreur}`);
        await enregistrerIncident(machine, typeErreur, detailErreur);

    } finally {
        client.close();
    }
}

// ==========================================================
// 🔄 BOUCLE PRINCIPALE (1 minute)
// ==========================================================
async function bouclePrincipale() {
    while (true) {
        console.log("\n----------------------------------------------------------");
        console.log(`🕒 Cycle de scrutation : ${new Date().toLocaleTimeString()}`);
        
        for (const machine of autoclaves) {
            await scruterAutoclave(machine);
        }

        console.log("----------------------------------------------------------");
        console.log("💤 Attente de 1 minute...");
        await sleep(60000); 
    }
}

// ==========================================================
// 🚀 LANCEMENT GLOBAL
// ==========================================================

// 1. Démarrage de l'écoute des requêtes Web (Nathan)
app.listen(3000, () => {
    console.log("👂 API Commande : En écoute sur le port 3000 (Prête pour le site web)");
});

// 2. Démarrage de la connexion BDD et de la boucle de scrutation
pool.getConnection()
    .then(conn => {
        console.log(`✅ Connexion MariaDB réussie (${process.env.DB_HOST})`);
        conn.release(); 
        bouclePrincipale();
    })
    .catch(err => {
        console.error(`❌ Échec connexion MariaDB : ${err.message}`);
    });