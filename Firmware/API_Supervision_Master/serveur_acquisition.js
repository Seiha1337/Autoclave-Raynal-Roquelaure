// Chargement du fichier caché .env pour sécuriser les identifiants
require('dotenv').config(); 
const ModbusRTU = require("modbus-serial");
const mysql = require("mysql2/promise");

console.log("==========================================================");
console.log(" 🖥️ SUPERVISION & HISTORISATION DES 6 AUTOCLAVES");
console.log("==========================================================");

// Pool de connexion à la base MariaDB de Noa
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

// Liste du matériel sur le réseau OT
const autoclaves = [
    { id: "Autoclave 1 (OPTA)", ip: "192.168.50.50", mac: "A8:61:0A:AE:76:05" },
    { id: "Autoclave 2 (ESP32)", ip: "192.168.50.51", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 3 (ESP32)", ip: "192.168.50.52", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 4 (ESP32)", ip: "192.168.50.53", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 5 (ESP32)", ip: "192.168.50.54", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 6 (ESP32)", ip: "192.168.50.55", mac: "XX:XX:XX:XX:XX:XX" }
];

// Fonction utilitaire pour gérer les pauses proprement
const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

async function scruterAutoclave(machine) {
    const client = new ModbusRTU();
    client.setTimeout(2000); // Timeout court pour ne pas bloquer la boucle si l'automate est down

    try {
        // Connexion et paramétrage Modbus TCP
        await client.connectTCP(machine.ip, { port: 502 });
        client.setID(1);

        // Lecture groupée : Température (0), État (1), Consigne (2)
        const data = await client.readHoldingRegisters(0, 3);
        
        // Remise à l'échelle (le C++ a multiplié par 10 pour envoyer un entier)
        const temperature = data.data[0] / 10.0;
        const etatMachine = data.data[1];
        const consigne = data.data[2] / 10.0;

        // Traduction du code machine en texte lisible pour le front de Nathan
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

        // --- SAUVEGARDE EN BASE DE DONNÉES ---
        try {
            const query = `
                INSERT INTO mesures_autoclaves 
                (autoclave_id, temperature, consigne, cycle, etat) 
                VALUES (?, ?, ?, ?, ?)
            `;
            
            // Extraction du numéro de la machine (ex: "Autoclave 1..." -> 1)
            const numeroAutoclave = machine.id.match(/\d+/)[0];
            
            await pool.execute(query, [numeroAutoclave, temperature, consigne, etatTexte, chauffeActuelle]);
            console.log(`    ↳ 💾 Données sauvegardées en BDD.`);
        } catch (dbError) {
            console.log(`    ↳ ⚠️ Erreur BDD: ${dbError.message}`);
        }

    } catch (e) {
        // Catch réseau si la machine ne répond pas au ping Modbus
        console.log(`[🔴 HORS LIGNE] ${machine.id} (IP: ${machine.ip}) - Erreur: ${e.message}`);
    } finally {
        // Nettoyage obligatoire du port pour le prochain tour
        client.close();
    }
}

// Boucle infinie de scrutation
async function bouclePrincipale() {
    while (true) {
        console.log("\n----------------------------------------------------------");
        console.log(`🕒 Début de la scrutation : ${new Date().toLocaleTimeString()}`);
        
        for (const machine of autoclaves) {
            await scruterAutoclave(machine);
        }

        console.log("----------------------------------------------------------");
        console.log("💤 Attente de 5 secondes...");
        await sleep(5000);
    }
}

// --- INIT ---
// Test du ping MariaDB avant de lancer l'usine à gaz
pool.getConnection()
    .then(conn => {
        console.log(`✅ Connexion à MariaDB (${process.env.DB_HOST}) réussie ! Lancement de la supervision...`);
        conn.release(); 
        bouclePrincipale();
    })
    .catch(err => {
        console.error(`❌ Impossible de se connecter à MariaDB sur ${process.env.DB_HOST}. Vérifier le réseau ou le .env`);
        console.error(err.message);
    });