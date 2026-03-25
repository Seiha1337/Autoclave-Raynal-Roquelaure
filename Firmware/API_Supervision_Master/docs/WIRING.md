# Schémas de câblage détaillés

## Schéma 1 : Connexion d'une carte WT32-ETH01 au module RS485

```
┌─────────────────────┐
│   WT32-ETH01        │
│                     │
│  GPIO17 (TX2) ──────┼───→ DI  ┐
│  GPIO5  (RX2) ──────┼───→ RO  │
│  GPIO4       ──────┼───→ DE  │  Module
│              ──────┼───→ RE  │  RS485
│  3.3V       ──────┼───→ VCC │  (MAX485)
│  GND        ──────┼───→ GND │
└─────────────────────┘         │
                                │
                          A ────┤
                          B ────┘
```

## Schéma 2 : Connexion complète des deux cartes

```
┌────────────────┐              ┌──────────┐              ┌────────────────┐
│  WT32-ETH01    │              │ RS485    │              │  WT32-ETH01    │
│   (ESCLAVE)    │              │  Bus     │              │   (MAÎTRE)     │
│                │              │          │              │                │
│  GPIO17 ────→ DI │            │          │            │ DI ←──── GPIO17 │
│  GPIO5  ────→ RO │            │          │            │ RO ←──── GPIO5  │
│  GPIO4  ────→ DE │  Module    │          │  Module    │ DE ←──── GPIO4  │
│         ────→ RE │  RS485-1   │          │  RS485-2   │ RE ←────        │
│                │              │          │              │                │
│                │   A ─────────┼──────────┼──────────── A  │                │
│                │   B ─────────┼──────────┼──────────── B  │                │
│                │   GND ───────┴──────────┴──────────── GND│                │
│                │              │          │              │                │
└────────────────┘         [120Ω]        [120Ω]         └────────────────┘
                          (optionnel)  (optionnel)
                              A-B          A-B
```

## Schéma 3 : Brochage du module RS485 MAX485

```
        MAX485 / MAX3485
      ┌─────────────────┐
   RO─┤1             8├─VCC (3.3V-5V)
  /RE─┤2             7├─B (Data -)
   DE─┤3             6├─A (Data +)
   DI─┤4             5├─GND
      └─────────────────┘
```

## Détails des connexions

### WT32-ETH01 → Module RS485

| Signal | Fonction | Direction |
|--------|----------|-----------|
| GPIO17 (TX2) | Transmission | WT32 → RS485 (DI) |
| GPIO5 (RX2) | Réception | RS485 (RO) → WT32 |
| GPIO4 | Contrôle direction | WT32 → RS485 (DE/RE) |

### Bus RS485 entre les modules

```
Module 1        Câble torsadé        Module 2
  A (Data+) ═══════════════════════════ A (Data+)
  B (Data-) ═══════════════════════════ B (Data-)
  GND       ─────────────────────────── GND
```

### Résistances de terminaison (recommandées)

Pour de meilleures performances, surtout si la distance entre les cartes est > 1m :

```
Module 1:         A ─┬─[120Ω]─┬─ B
                      │        │
                     GND      GND

Module 2:         A ─┬─[120Ω]─┬─ B
                      │        │
                     GND      GND
```

## Notes importantes

1. **Polarité du bus RS485** : 
   - A = Data+ (souvent appelé aussi D+)
   - B = Data- (souvent appelé aussi D-)
   - Ne pas inverser !

2. **Distance maximale** :
   - Sans terminaison : ~10 mètres
   - Avec terminaison : jusqu'à 1200 mètres à 9600 bauds

3. **Nombre de dispositifs** :
   - Un bus RS485 peut supporter jusqu'à 32 dispositifs (avec des drivers standards)
   - Utiliser des répéteurs ou des drivers spéciaux pour plus de dispositifs

4. **Alimentation** :
   - Les modules MAX485 peuvent fonctionner en 3.3V ou 5V
   - Vérifier la tension d'alimentation selon le module utilisé
   - L'ESP32 (WT32-ETH01) fonctionne en logique 3.3V

5. **Masse commune** :
   - TOUJOURS connecter les GND des deux modules ensemble
   - C'est essentiel pour avoir une référence de tension commune
