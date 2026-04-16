const ModbusRTU = require("modbus-serial");

// Extraction des arguments : node cmd_usine.js <fin_IP> <action>
const ipFinale = process.argv[2]; 
const action = process.argv[3];   

if (!ipFinale || !action) {
    console.log("\n--- AIDE COMMANDE USINE ---");
    console.log("Utilisation : node cmd_usine.js <IP> <action>");
    console.log("Actions : 'marche', 'arret', ou un nombre (ex: 125)");
    console.log("---------------------------\n");
    process.exit(1);
}

const ipTarget = `172.40.45.${ipFinale}`;
const client = new ModbusRTU();

async function executer() {
    try {
        await client.connectTCP(ipTarget, { port: 502 });
        client.setID(1);

        if (action === "marche") {
            await client.writeRegister(1, 1); // REG_STATE = 1
            console.log(`✅ [${ipTarget}] Ordre MARCHE envoyé.`);
        } 
        else if (action === "arret") {
            await client.writeRegister(1, 0); // REG_STATE = 0 (Force Repos/Refroidissement)
            console.log(`🛑 [${ipTarget}] Ordre ARRÊT envoyé.`);
        } 
        else if (!isNaN(action)) {
            const val = Math.round(parseFloat(action) * 10);
            await client.writeRegister(2, val); // REG_CONSIGNE = valeur * 10
            console.log(`🌡️ [${ipTarget}] Nouvelle consigne : ${action}°C`);
        }
    } catch (e) {
        console.log(`❌ Erreur sur ${ipTarget} : ${e.message}`);
    } finally {
        client.close();
    }
}

executer();