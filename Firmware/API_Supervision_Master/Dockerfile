# Utilise la version allégée de Node.js 24 (la version que tu utilises)
FROM node:24-alpine

# Crée le dossier de l'application dans le conteneur
WORKDIR /usr/src/app

# Copie les fichiers de dépendances en premier (pour optimiser le cache Docker)
COPY package*.json ./

# Installe les dépendances (modbus-serial, mysql2, dotenv)
RUN npm install

# Copie le reste de ton code source (ton serveur_acquisition.js)
COPY . .

# Démarre le script
CMD [ "node", "serveur_acquisition.js" ]