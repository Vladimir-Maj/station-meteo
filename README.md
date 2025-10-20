# station-meteo

Prototype de station météo embarquée (Arduino ATmega328) pour navires : acquisition capteurs (PTU, GPS, luminosité), horodatage RTC, journalisation SD (rotation), 4 modes (Standard/Config/Éco/Maintenance), interruptions boutons, code optimisé mémoire/énergie, scripts build/flash.

## 🚢 Vue d’ensemble
- Mesures : **Température**, **Pression**, **Humidité**, **Luminosité**, **GPS**  
- Horodatage via **RTC** • Stockage **SD** • **LED RGB** pour l’état/erreurs  
- **Modes** accessibles par **boutons** • **Console série** pour la configuration

## 🔧 Matériel & câblage (exemple)
- **I²C** : RTC, BME280  
- **A1** : capteur luminosité (LDR)  
- **D2** : bouton (appui long 5 s)  
- **D7** : LED RGB (ou driver/strip suivant montage)  
- **GPS** : via SoftwareSerial (ex. D4 + une autre pin RX/TX)  
- **SD** : shield SPI

> Adapte les pins selon ta carte; conserve I²C pour RTC/BME280 et SPI pour SD.

## 🧭 Modes & LED
- **STANDARD** (LED **verte**) : acquisition périodique, écriture SD  
- **CONFIGURATION** (LED **jaune**) : paramètres via console série  
- **MAINTENANCE** (LED **orange**) : affichage direct, **pas** d’écriture SD  
- **ÉCONOMIE** (LED **bleue**) : intervalle ×2, GPS 1 fois sur 2

**Boutons**
- **Au boot + bouton rouge** → CONFIGURATION  
- **Depuis STANDARD** :  
  - **Rouge 5 s** → MAINTENANCE (re-appui 5 s pour revenir)  
  - **Vert 5 s** → ÉCONOMIE (rouge 5 s pour revenir)

**Tableau des états LED**
| Couleur et fréquence du signal lumineux | État du système |
|---|---|
| LED verte continue | Mode standard |
| LED jaune continue | Mode configuration |
| LED bleue continue | Mode économique |
| LED orange continue | Mode maintenance |
| LED **intermittente rouge et bleue** (1 Hz, durée identique) | Erreur d’accès à l’horloge RTC |
| LED **intermittente rouge et jaune** (1 Hz, durée identique) | Erreur d’accès aux données du GPS |
| LED **intermittente rouge et verte** (1 Hz, durée identique) | Erreur d’accès aux données d’un capteur |
| LED **intermittente rouge et verte** (1 Hz, **vert 2× plus long**) | Données incohérentes d’un capteur — vérification matérielle requise |
| LED **intermittente rouge et blanche** (1 Hz, durée identique) | Carte SD pleine |
| LED **intermittente rouge et blanche** (1 Hz, **blanc 2× plus long**) | Erreur d’accès/écriture sur la carte SD |

## ⚙️ Paramètres (console série)
Persistants en **EEPROM**. Exemples :
```
LOG_INTERVAL=10        # minutes (défaut 10)
FILE_MAX_SIZE=2048     # octets (ex. 4096 possible)
TIMEOUT=30             # secondes (défaut 30)

# Seuils capteurs
LUMIN=1; LUMIN_LOW=100; LUMIN_HIGH=900
TEMP_AIR=1; MIN_TEMP_AIR=-10; MAX_TEMP_AIR=50
HYGR=1; HYGR_MINT=-10; HYGR_MAXT=50
PRESSURE=1; PRESSURE_MIN=300; PRESSURE_MAX=1100

# RTC
CLOCK 12:34:56
DATE 04,15,2025
DAY MON

RESET                # réinitialiser paramètres
VERSION              # afficher version firmware
```

## 💾 Journalisation SD
- Fichier courant : **`YYMMDD_rev.LOG`** (écriture dans **rev0**)  
- Quand `FILE_MAX_SIZE` atteint → **copie** en `rev+1`, **réinitialisation** de `rev0`  
- Lignes compactes horodatées (CSV/TSV suivant build)


## 🚀 Build & flash
- **Arduino IDE** : ouvrir le projet, choisir la carte/port, téléverser.  
- **Ou** scripts (Linux/macOS) :
```bash
scripts/build.sh   # compile (optimisation -Os)
scripts/flash.sh   # téléverse
```

