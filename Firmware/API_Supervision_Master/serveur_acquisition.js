require('dotenv').config(); // 👈 NOUVEAU : Charge les variables du fichier .env
const ModbusRTU = require("modbus-serial");
const mysql = require("mysql2/promise");

console.log("==========================================================");
console.log(" 🖥️ SUPERVISION & HISTORISATION DES 6 AUTOCLAVES");
console.log("==========================================================");

// --- CONFIGURATION DE LA BASE DE DONNÉES SÉCURISÉE ---
const pool = mysql.createPool({
    host: process.env.DB_HOST,         // 👈 Récupère l'IP depuis le .env
    port: process.env.DB_PORT,         // 👈 Récupère le port
    user: process.env.DB_USER,         // 👈 Récupère l'utilisateur
    password: process.env.DB_PASSWORD, // 👈 Récupère le mot de passe caché
    database: process.env.DB_NAME,     // 👈 Récupère le nom de la BDD
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
});

const autoclaves = [
    { id: "Autoclave 1 (OPTA)", ip: "192.168.50.50", mac: "A8:61:0A:AE:76:05" },
    { id: "Autoclave 2 (ESP32)", ip: "192.168.50.51", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 3 (ESP32)", ip: "192.168.50.52", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 4 (ESP32)", ip: "192.168.50.53", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 5 (ESP32)", ip: "192.168.50.54", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 6 (ESP32)", ip: "192.168.50.55", mac: "XX:XX:XX:XX:XX:XX" }
];

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

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

        // --- INJECTION DANS LA BASE DE DONNÉES FUSIONNÉE ---
        try {
            const query = `
                INSERT INTO mesures_autoclaves 
                (autoclave_id, temperature, consigne, cycle, etat) 
                VALUES (?, ?, ?, ?, ?)
            `;
            
            const numeroAutoclave = machine.id.match(/\d+/)[0];
            
            await pool.execute(query, [numeroAutoclave, temperature, consigne, etatTexte, chauffeActuelle]);
            console.log(`    ↳ 💾 Données sauvegardées en BDD.`);
        } catch (dbError) {
            console.log(`    ↳ ⚠️ Erreur BDD: ${dbError.message}`);
        }

    } catch (e) {
        console.log(`[🔴 HORS LIGNE] ${machine.id} (IP: ${machine.ip}) - Erreur: ${e.message}`);
    } finally {
        client.close();
    }
}

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

// --- DÉMARRAGE DU PROGRAMME ---
pool.getConnection()
    .then(conn => {
        console.log(`✅ Connexion à MariaDB (${process.env.DB_HOST}) réussie ! Lancement de la supervision...`);
        conn.release(); 
        bouclePrincipale();
    })
    .catch(err => {
        console.error(`❌ Impossible de se connecter à MariaDB sur ${process.env.DB_HOST}.`);
        console.error(err.message);
    });