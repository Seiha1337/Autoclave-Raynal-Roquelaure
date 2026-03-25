const ModbusRTU = require("modbus-serial");

console.log("==========================================================");
console.log(" 🖥️ SCADA PC : SUPERVISION DES 6 AUTOCLAVES EN DIRECT");
console.log("==========================================================");

// Ton plan d'adressage (Tu pourras compléter les vraies adresses MAC des ESP32 plus tard)
const autoclaves = [
    { id: "Autoclave 1 (OPTA)", ip: "192.168.50.50", mac: "A8:61:0A:AE:76:05" },
    { id: "Autoclave 2 (ESP32)", ip: "192.168.50.51", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 3 (ESP32)", ip: "192.168.50.52", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 4 (ESP32)", ip: "192.168.50.53", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 5 (ESP32)", ip: "192.168.50.54", mac: "XX:XX:XX:XX:XX:XX" },
    { id: "Autoclave 6 (ESP32)", ip: "192.168.50.55", mac: "XX:XX:XX:XX:XX:XX" }
];

async function scruterAutoclave(machine) {
    const client = new ModbusRTU();
    client.setTimeout(2000); // On n'attend pas plus de 2s si la machine est éteinte

    try {
        // Connexion à l'autoclave
        await client.connectTCP(machine.ip, { port: 502 });
        client.setID(1);

        // Lecture de la Température (0), l'État (1) et la Consigne (2)
        const data = await client.readHoldingRegisters(0, 3);
        
        const temperature = data.data[0] / 10.0;
        const etatMachine = data.data[1];
        const consigne = data.data[2] / 10.0;

        // Traduction de l'état pour savoir si ça chauffe
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

        // Affichage formaté dans le terminal
        console.log(`[🟢 EN LIGNE] ${machine.id}`);
        console.log(`    ├─ Réseau   : IP ${machine.ip} | MAC ${machine.mac}`);
        console.log(`    ├─ Process  : Temp = ${temperature}°C | Consigne = ${consigne}°C`);
        console.log(`    └─ Machine  : Cycle = ${etatTexte} | Relais Chauffe = ${chauffeActuelle}\n`);

        client.close();

    } catch (e) {
        console.log(`[🔴 HORS LIGNE] ${machine.id} (IP: ${machine.ip}) - Erreur: ${e.message}\n`);
    }
}

// Lancement de la scrutation toutes les 5 secondes
setInterval(() => {
    console.log("----------------------------------------------------------");
    autoclaves.forEach(machine => scruterAutoclave(machine));
}, 5000);