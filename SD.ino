#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

// RTC DS3231
RTC_DS3231 rtc;

// SD card
const int chipSelect = 4;

const char *daysOfTheWeek[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"}; 

//stocker valeurs dans EEPROM
void writeEEPROM(int address, int value) {
  EEPROM.write(address, value);
}

//lire valeurs de EEPROM
int readEEPROM(int address) {
  return EEPROM.read(address);
}



void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Ini du RTC
  if (!rtc.begin()) {
    Serial.println("RTCpala");
    while (1);
  }

  // Vérif état RTC
  if (rtc.lostPower()) {
    Serial.println("RTCpaco");
    rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0)); // Réglage par défaut
  }

  // Ini shield SD
  if (!SD.begin(chipSelect)) {
    Serial.println("erreur SD");//sa peut venirdu le shield ou la carte
    while (1); // Boucle infinie si shield SD pas prêt
  }
  Serial.println(" Carte SD prête.");

  Serial.println("Commandes disponibles : CLOCK, DATE, DAY, GET_RTC, LOG_DATA");
}



//récupérer et afficher les données RTC
void getRTCData() {
  DateTime now = rtc.now();


  Serial.print("Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  Serial.print("Date: ");
  Serial.print(now.day());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.println(now.year());

  Serial.print("Day: ");
  Serial.println(daysOfTheWeek[now.dayOfTheWeek()]);
}

// enregistrer données dans fichier CSV dans la carte SD
void logDataToSD(String sensorData) {
  if (!SD.begin(chipSelect)) { // Vérifie carte SD toujours présente
    Serial.println("Erreur : Carte SD absente ou retirée.");
    return;
  }

  File dataFile = SD.open("data.csv", FILE_WRITE);

  if (dataFile) {
    DateTime now = rtc.now();
    dataFile.print(now.year());
    dataFile.print("-");
    dataFile.print(now.month());
    dataFile.print("-");
    dataFile.print(now.day());
    dataFile.print(" ");
    dataFile.print(now.hour());
    dataFile.print(":");
    dataFile.print(now.minute());
    dataFile.print(":");
    dataFile.print(now.second());
    dataFile.print(",");
    dataFile.println(sensorData);

    dataFile.close();
    Serial.println("Données enregistrées dans data.csv.");
  } else {
    Serial.println("Erreur : Impossible d'écrire dans fichier data.csv.");
  }
}



//traiter les commandes
void handleCommand(String command) {
  command.trim(); // Supprime les espaces inutiles

  if (command.startsWith("CLOCK")) {
    int hour, minute, second;
    if (sscanf(command.c_str(), "CLOCK %d:%d:%d", &hour, &minute, &second) == 3) {
      if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
        rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour, minute, second));
        writeEEPROM(0, hour);
        writeEEPROM(1, minute);
        writeEEPROM(2, second);
        Serial.println("Heure mise à jour.");
      } else {
        Serial.println("Erreur : Valeurs invalides");
      }
    } else {
      Serial.println("Erreur : Format de commande incorrect");
    }
  } else if (command.startsWith("DATE")) {
    int month, day, year;
    if (sscanf(command.c_str(), "DATE %d,%d,%d", &month, &day, &year) == 3) {
      if (month >= 1 && month <= 12 && day >= 1 && day <= 31 && year >= 2000 && year <= 2099) {
        rtc.adjust(DateTime(year, month, day, rtc.now().hour(), rtc.now().minute(), rtc.now().second()));
        writeEEPROM(3, month);
        writeEEPROM(4, day);
        writeEEPROM(5, year - 2000);
        Serial.println("Date mise à jour.");
      } else {
        Serial.println("Erreur : Valeurs invalides pour la date.");
      }
    } else {
      Serial.println("Erreur : Format de commande incorrect.");
    }
  } else if (command.equalsIgnoreCase("GET_RTC")) {
    getRTCData();
  } else if (command.startsWith("LOG_DATA")) {
    String sensorData = command.substring(9);
    logDataToSD(sensorData);
  } else {
    Serial.println("Erreur : Commande inconnue.");
  }
}


void loop() {
  if (Serial.available()) {
    String command = Serial.readString(); // Lire la commande d'entrée 
    handleCommand(command); 
  }
}
