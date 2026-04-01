require('dotenv').config(); 
const ModbusRTU = require("modbus-serial");
const mysql = require("mysql2/promise");

// ==========================================================
// 🛡️ BOUCLIER ANTI-CRASH GLOBAL (Spécifique IoT industriel)
// Empêche Node.js de planter si une machine débranchée 
// renvoie une erreur réseau "en retard" après le TimeOut.
// ==========================================================
process.on('uncaughtException', (err) => {
    // On ignore silencieusement les erreurs de connexion refusée
    if (err.code !== 'ECONNREFUSED' && err.code !== 'EHOSTUNREACH' && err.code !== 'ETIMEDOUT') {
        console.error("⚠️ Erreur critique interceptée :", err.message);
    }
});

process.on('unhandledRejection', (reason, promise) => {
    // On capture les promesses orphelines de la librairie Modbus
});

console.log("==========================================================");
console.log(" 🖥️ SUPERVISION & HISTORISATION DES 6 AUTOCLAVES");
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
            console.log(`    ↳ 💾 Données sauvegardées en BDD.`);
        } catch (dbError) {
            console.log(`    ↳ ⚠️ Erreur BDD: ${dbError.message}`);
        }

    } catch (e) {
        console.log(`[🔴 HORS LIGNE] ${machine.id} (IP: ${machine.ip}) - Indisponible`);
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
        console.log("💤 Attente de 1 minute avant la prochaine scrutation...");
        await sleep(60000); // 60 000 millisecondes = 1 minute
    }
}

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