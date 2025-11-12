#include <Arduino.h>
#include <ChainableLED.h>
#include <Adafruit_BME280.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>


// Paramètre généraux avec paramètre variable
#define LUMIN 1
#define LUMIN_LOW 200
#define LUMIN_HIGH 700

#define TEMP_AIR 1
#define MIN_TEMP_AIR -10
#define MAX_TEMP_AIR 40

#define HYGR 1
#define HYGR_MINT 20
#define HYGR_MAXT 80

#define PRESSURE 1
#define PRESSURE_MIN 900
#define PRESSURE_MAX 1100

#define LOG_INTERVAL 10
#define FILE_MAX_SIZE 4096

#define NUM_LEDS 1
ChainableLED leds(8, 9, NUM_LEDS);

const PROGMEM int pinLight = A1;
RTC_DS3231 rtc;

int lumin = LUMIN;
int luminLow = LUMIN_LOW;
int luminHigh = LUMIN_HIGH;
int tempAir = TEMP_AIR;
int minTempAir = MIN_TEMP_AIR;
int maxTempAir = MAX_TEMP_AIR;
int hygr = HYGR;
int hygrMinT = HYGR_MINT;
int hygrMaxT = HYGR_MAXT;
int pressure = PRESSURE;
int pressureMin = PRESSURE_MIN;
int pressureMax = PRESSURE_MAX;
int logInterval = LOG_INTERVAL;
int fileMaxSize = FILE_MAX_SIZE;

#define adresseI2CduBME280 0x76
#define pressionAuNiveauDeLaMerEnHpa 1024.90
#define delaiRafraichissementAffichage 1500
Adafruit_BME280 bme;
SoftwareSerial SoftSerial(4, 5); //Initi port série logiciel pour  GPS

const PROGMEM int buttonRed = 3;
const PROGMEM int buttonGreen = 2;
const PROGMEM int LED = 7;
volatile bool buttonRedPressed = false;
volatile bool buttonGreenPressed = false;
volatile bool stateRed = 1;
volatile bool stateGreen = 1;
volatile unsigned long buttonRedPressedTime = 0;
volatile unsigned long buttonGreenPressedTime = 0;
unsigned long buttonRedPressStartTime = 0;
unsigned long buttonGreenPressStartTime = 0;
const PROGMEM unsigned long pressDuration = 3000;
int currentTime;

enum Mode { CONFIGURATION, STANDARD, ECONOMIE, MAINTENANCE };
Mode currentMode = STANDARD; // démarage en standard


//decla struc

bool tookvalue = 0;


struct Clock {
    String date;
    String time;
};

struct Coords {
    double latitude;
    double longitude;
};

struct Result {
    Clock date;
    float luminosity;
    float temperature;
    float humidity;
    float pressure;
    Coords gps;
};

Result mesuresa;

enum ErrorType {
    RTC_ERROR,
    GPS_ERROR,
    SENSOR_ACCESS_ERROR,
    SENSOR_DATA_ERROR,
    SD_CARD_FULL,
    SD_CARD_ACCESS_ERROR
};


void bouton() {
  static bool bootChecked = false;
  if (!bootChecked) {
    if (millis() < 5000 && digitalRead(buttonRed) == LOW && currentMode == STANDARD) {
      Serial.println(F("Démarrage: bouton rouge maintenu -> mode CONFIGURATION"));
      ChangeMode(CONFIGURATION);
    }
    if (millis() >= 5000) bootChecked = true;
  }

  // États internes (retenus entre deux appels)
  static bool redDown = false, greenDown = false;
  static bool redTrig = false, greenTrig = false;
  static uint32_t redStart = 0, greenStart = 0;

  const uint32_t now = millis();

  // rouge
  bool r = (digitalRead(buttonRed) == LOW); // INPUT_PULLUP : appui = LOW
  if (r && !redDown) {               // début d'appui
    redDown = true; redTrig = false; redStart = now;
  }
  if (!r && redDown) {               // relâche
    redDown = false; redTrig = false;
  }
  if (redDown && !redTrig && (now - redStart >= pressDuration)) {
    redTrig = true;                  // déclenché une seule fois par appui long
    switch (currentMode) {
      case STANDARD:
        ChangeMode(MAINTENANCE);
        break;
      case ECONOMIE:                 // accès à MAINTENANCE aussi depuis ECO
        ChangeMode(MAINTENANCE);
        break;
      case MAINTENANCE:
        ChangeMode(STANDARD);        // retour simplifié vers STANDARD
        break;
      case CONFIGURATION:
        ChangeMode(STANDARD);
        break;
      default:
        break;
    }
  }

  // vert
  bool g = (digitalRead(buttonGreen) == LOW); // INPUT_PULLUP : appui = LOW
  if (g && !greenDown) {               // début d'appui
    greenDown = true; greenTrig = false; greenStart = now;
  }
  if (!g && greenDown) {               // relâche
    greenDown = false; greenTrig = false;
  }
  if (greenDown && !greenTrig && (now - greenStart >= pressDuration)) {
    greenTrig = true;                  // déclenché une seule fois par appui long
    switch (currentMode) {
      case STANDARD:
        Serial.println(F("Passage en mode ECONOMIE"));
        ChangeMode(ECONOMIE);
        break;
      case ECONOMIE:
        Serial.println(F("Passage en mode STANDARD"));
        ChangeMode(STANDARD);
        break;
      default:                         // MAINTENANCE / CONFIGURATION -> pas d'effet
        break;
    }
  }
}


void UpdateModes() {
    if (currentMode == STANDARD) {
      if (millis()-currentTime < 500 || millis()-currentTime > 60000*logInterval){
        getData(&mesuresa);
        currentTime=millis();
        //Write data on SD;
      }
      if (millis()-currentTime > 2000 && millis()-currentTime <2100){
        getData(&mesuresa);
        currentTime=millis();
      }
    } else if (currentMode == ECONOMIE) {
        if (millis()-currentTime < 500 || millis()-currentTime > 60000*logInterval*2){
          getData(&mesuresa);
          //Write data on SD;
        }
        if (millis()-currentTime > 2000 && millis()-currentTime <2100){
          getData(&mesuresa);
          currentTime=millis();
        }
    }
      else if (currentMode == MAINTENANCE) {
        delay(2000);
        getData(&mesuresa);

        Serial.print(F("Date : "));
        Serial.println(mesuresa.date.date);

        Serial.print(F("Heure : "));
        Serial.println(mesuresa.date.time);

        Serial.print(F("Luminosité = "));
        Serial.println(mesuresa.luminosity);

        Serial.print(F("Température = "));
        Serial.print(mesuresa.temperature);
        Serial.println(F(" °C"));

        Serial.print(F("Humidité = "));
        Serial.print(mesuresa.humidity);
        Serial.println(F(" %"));

        Serial.print(F("Pression atmosphérique = "));
        Serial.print(mesuresa.pressure);
        Serial.println(F(" hPa"));

        Serial.print(F("Coordonnées = "));
        Serial.print(mesuresa.gps.latitude,6);
        Serial.print(F(","));
        Serial.println(mesuresa.gps.longitude,6);
        Serial.println();
    } else if (currentMode == CONFIGURATION) {
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            handleCommand(command);
        } else if (millis() - currentTime > 300000) {
            Serial.print(F("Passage en mode STANDARD\n"));
            ChangeMode(STANDARD);
        }
    } else {
        Serial.println(F("UNKNOWN_MODE"));
    }
}

void handleButtonRedPress() {
    buttonRedPressStartTime = millis();
    buttonRedPressed = true;
}

void handleButtonGreenPress() {
    buttonGreenPressStartTime = millis();
    buttonGreenPressed = true;
}

void ChangeMode(Mode mode) {
    currentTime = millis();
    currentMode = mode;
}


//LED pour identifier mode et erreur système
void Leds(Mode currentMode) {
  switch (currentMode) {
    case STANDARD:
      setAllLedsColorHSL(0.33, 1.0, 0.5); // Vert
      break;
    case CONFIGURATION:
      setAllLedsColorHSL(0.16, 1.0, 0.5); // Jaune
      break;
    case MAINTENANCE:
      setAllLedsColorHSL(0.08, 1.0, 0.5); // Orange
      break;
    case ECONOMIE:
      setAllLedsColorHSL(0.66, 1.0, 0.5); // Bleu
      break;
    default:
      Serial.println(F("Erreur"));
      break;
  }
}

void displayError(ErrorType error) {
    switch (error) {
        case RTC_ERROR:
            while(!rtc.begin()){
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(0.66, 1.0, 0.5); // Bleu
                delay(500);
                }
        case GPS_ERROR:
            while(isnan(getGPS(false).latitude) || getGPS(false).latitude==NULL){
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(0.16, 1.0, 0.5); // Jaune
                delay(500);
            }
            break;
        case SENSOR_ACCESS_ERROR:
            while (isnan(getLumin()) || isnan(getTemperature()) || isnan(getHumidity()) || isnan(getPressure())) {
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(0.33, 1.0, 0.5); // Vert
                delay(500);
            }
            break;
        case SENSOR_DATA_ERROR:
            while (getLumin()==0 || getTemperature()==0 || getHumidity()==0 || getPressure()==0) {
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(0.33, 1.0, 0.5); // Vert
                delay(1000);
            }
            break;
        case SD_CARD_FULL:
            while (true) {
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(1.0, 0.0, 1.0); // Blanc
                delay(500);
            }
            break;
        case SD_CARD_ACCESS_ERROR:
            while (true) {
                setAllLedsColorHSL(0.0, 1.0, 0.5); // Rouge
                delay(500);
                setAllLedsColorHSL(1.0, 0.0, 1.0); // Blanc
                delay(1000);
            }
            break;
        default:
            Serial.println(F("Erreur inconnu"));
            break;
    }
}


void setAllLedsColorHSL(float hue, float saturation, float lightness) {
    for (byte i = 0; i < NUM_LEDS; i++) {
        leds.setColorHSL(i, hue, saturation, lightness);
    }
}

//capteur lumière avec domaine définition de valeur
float getLumin() {
    float lumin = analogRead(pinLight);

    if (lumin >= LUMIN_LOW && lumin <= LUMIN_HIGH) {
        return lumin;
    } else {
        return NAN;
    }
}

//capteur BME
float getTemperature() {
    float temperature = bme.readTemperature();
    if (temperature >= MIN_TEMP_AIR && temperature <= MAX_TEMP_AIR) {
        return temperature;
    } else {
        return NAN;
    }
}

float getHumidity() {
    float humidity = bme.readHumidity();
    if (humidity >= HYGR_MINT && humidity <= HYGR_MAXT) {
        return humidity;
    } else {
        return NAN;
    }
}

float getPressure() {
    float pressure = (bme.readPressure() / 100.0F);
    if (pressure >= PRESSURE_MIN && pressure <= PRESSURE_MAX) {
        return pressure;
    } else {
        return NAN;
    }
}

//GPS
struct Clock getRTCData() {
    if (rtc.begin()) {
        char buf1[11];
        char buf2[7];

        sprintf(buf1, "%02d/%02d/%02d", rtc.now().day(), rtc.now().month(), rtc.now().year());
        sprintf(buf2, "%02d:%02d:%02d", rtc.now().hour(), rtc.now().minute(), rtc.now().second());
        return {buf1, buf2};
    } else {
        return {"NAN", "NAN"};
    }
}

//convertir coord GPS
float ConvertData(double rawDegrees, String cardinal)
{
  double DD =int(double(rawDegrees)/100);
  double MM = double(rawDegrees) - int(double(rawDegrees)/100) * 100;
  double deg = (DD + MM/60);
  if (cardinal == "S" || cardinal == "W"){
    return (-deg);
  }
  else
  {
    return deg;
  }
}

struct Coords getGPS(bool ecoMode) {
    if ((ecoMode && tookvalue==false)!=(ecoMode==false)){
      char gpsData[128];//tab stock trame nmea
      int count = 0;
      int sentenceComplete = 0;
        while (SoftSerial.available()) {
            char c = SoftSerial.read();//lit cara gps

            if(sentenceComplete==2){//2lign donnée gp
              break;
            }
            if (c == '\n' || count >= 127) {
                gpsData[count] = '\0';//fintrame
                sentenceComplete ++;
            } else {
                gpsData[count++] = c;//carac buffer
            }
        }
        char *strings[50];
        char *ptr = NULL;
        byte index = 0;
          ptr = strtok(gpsData, ",");
          while(ptr != NULL)
          {
              strings[index] = ptr;
              index++;
              ptr = strtok(NULL, ",");
          }
          for(int n = 0; n < index; n++)
          { 
            if(strstr(strings[n], "$GPGGA"))
            {
              double lat = strtod(strings[n+2],&strings[n+2]);
              String latc = strings[n+3];

              double lng = strtod(strings[n+4],&strings[n+4]);
              String lngc = strings[n+5];
              if (lat == 0 || lng == 0){
                return {NAN,NAN};
                break;
              }
              else{
                lat = ConvertData(lat,latc);
                lng = ConvertData(lng,lngc);
                if (lat == 0 || lng == 0){
                  return {NAN,NAN};
                  break;
                }
                else{
                  String loc = String(lat,6)+","+String(lng,6);
                  return {lat,lng};
                  break;
                }
              }
            }

            if(String(strings[n])==String(strings[index-1]))
            {
              return {NAN, NAN};
              break;
            }
          }
        
        for (int i = 0; i < 128; i++) {
            gpsData[i] = '\0';
        }
        count = 0;
        tookvalue=true;
        }
  else if(ecoMode && tookvalue){
    tookvalue=false;
    return {mesuresa.gps.latitude,mesuresa.gps.longitude};
  }
}


//configuration de l'EEPROM
void handleCommand(String command) {
    command.trim();

    if (command.startsWith("LOG_INTERVAL=")) {
        logInterval=command.substring(14).toInt();
        EEPROM.put(0, LOG_INTERVAL);
        Serial.println(F("LOG_INTERVAL mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("FILE_MAX_SIZE=")) {
        fileMaxSize = command.substring(14).toInt();
        EEPROM.put(4, FILE_MAX_SIZE);
        Serial.println(F("FILE_MAX_SIZE mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("LUMIN=")) {
        lumin = command.substring(6).toInt();
        EEPROM.put(8, LUMIN);
        Serial.println(F("LUMIN mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("LUMIN_LOW=")) {
        luminLow = command.substring(10).toInt();
        EEPROM.put(12, LUMIN_HIGH);
        Serial.println(F("LUMIN_LOW mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("LUMIN_HIGH=")) {
        luminHigh = command.substring(11).toInt();
        EEPROM.put(16, LUMIN_HIGH);
        Serial.println(F("LUMIN_HIGH mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("TEMP_AIR=")) {
        tempAir = command.substring(9).toInt();
        EEPROM.put(20, TEMP_AIR);
        Serial.println(F("TEMP_AIR mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("MIN_TEMP_AIR=")) {
        minTempAir = command.substring(14).toInt();
        EEPROM.put(24, MIN_TEMP_AIR);
        Serial.println(F("MIN_TEMP_AIR mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("MAX_TEMP_AIR=")) {
        maxTempAir = command.substring(14).toInt();
        EEPROM.put(28, MAX_TEMP_AIR);
        Serial.println(F("MAX_TEMP_AIR mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("HYGR=")) {
        hygr = command.substring(5).toInt();
        EEPROM.put(32, HYGR);
        Serial.println(F("HYGR mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("HYGR_MINT=")) {
        hygrMinT = command.substring(9).toInt();
        EEPROM.put(36, HYGR_MINT);
        Serial.println(F("HYGR_MINT mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("HYGR_MAXT=")) {
        hygrMaxT = command.substring(9).toInt();
        EEPROM.put(40, HYGR_MAXT);
        Serial.println(F("HYGR_MAXT mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("PRESSURE=")) {
        pressure = command.substring(9).toInt();
        EEPROM.put(44, PRESSURE);
        Serial.println(F("PRESSURE mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("PRESSURE_MIN=")) {
        pressureMin = command.substring(13).toInt();
        EEPROM.put(48, PRESSURE_MIN);
        Serial.println(F("PRESSURE_MIN mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("PRESSURE_MAX=")) {
        pressureMax = command.substring(13).toInt();
        EEPROM.put(52, PRESSURE_MAX);
        Serial.println(F("PRESSURE_MAX mis à jour."));
        currentTime=millis();
    } else if (command.startsWith("CLOCK=")) {
        int hour, minute, second;
        if (sscanf(command.c_str(), "CLOCK=%d:%d:%d", &hour, &minute, &second) == 3) {
          if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
            rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour, minute, second));
            Serial.println(F("Heure mise à jour"));
          } else {
            Serial.println(F("Erreur de valeur"));
          }
        } else {
          Serial.println(F("Format incorrect ! Utilisez : CLOCK=HH:MM:SS"));
        }
        currentTime = millis();
    } else if (command.startsWith("DATE=")) {
        int month, day, year;
        if (sscanf(command.c_str(), "DATE=%d/%d/%d", &day, &month, &year) == 3) {
          if (month >= 1 && month <= 12 && day >= 1 && day <= 31 && year >= 2000 && year <= 2099) {
            rtc.adjust(DateTime(year, month, day, rtc.now().hour(), rtc.now().minute(), rtc.now().second()));
            Serial.println(F("Date mise à jour"));
          } else {
            Serial.println(F("Erreur de valeur"));
          }
        } else {
          Serial.println(F("Format incorrect ! Utilisez : DATE=JJ/MM/AAAA"));
        } 
        currentTime = millis();
    } else {
        Serial.println(F("Commande inconnue."));
    }
}

//sauvegarde et lect
void loadConfigFromEEPROM() {
    EEPROM.get(0, logInterval);
    EEPROM.get(4, fileMaxSize);
    EEPROM.get(8, lumin);
    EEPROM.get(12, luminLow);
    EEPROM.get(16, luminHigh);
    EEPROM.get(20, tempAir);
    EEPROM.get(24, minTempAir);
    EEPROM.get(28, maxTempAir);
    EEPROM.get(32, hygr);
    EEPROM.get(36, hygrMinT);
    EEPROM.get(40, hygrMaxT);
    EEPROM.get(44, pressure);
    EEPROM.get(48, pressureMin);
    EEPROM.get(52, pressureMax);
}

//données -> stock et verif sinon error via led
void getData(Result *array) {
    array->date = getRTCData();
    if (array->date.date == "NAN" || array->date.time == "NAN") {
        displayError(RTC_ERROR);
    }

    array->luminosity = getLumin();
    if (isnan(array->luminosity)) {
        displayError(SENSOR_ACCESS_ERROR);
    }

    array->temperature = getTemperature();
    if (isnan(array->temperature)) {
        displayError(SENSOR_ACCESS_ERROR);
    }

    array->humidity = getHumidity();
    if (isnan(array->humidity)) {
        displayError(SENSOR_ACCESS_ERROR);
    }

    array->pressure = getPressure();
    if (isnan(array->pressure)) {
        displayError(SENSOR_ACCESS_ERROR);
    }

    array->gps = getGPS(false);
    if (isnan(array->gps.latitude) || array->gps.latitude==NULL || isnan(array->gps.longitude) || array->gps.longitude==NULL) {
        displayError(GPS_ERROR);
    }
}



void setup() {
    delay(3000);
    SoftSerial.begin(9600);
    leds.init();
    pinMode(buttonRed, INPUT_PULLUP);
    pinMode(buttonGreen, INPUT_PULLUP);
    pinMode(LED, OUTPUT);
    Serial.begin(9600);
    loadConfigFromEEPROM();

    if (!rtc.begin()) {
        Serial.println("RTC non connectée");
        displayError(RTC_ERROR);
    }
    if (rtc.lostPower()) {
        Serial.println("Erreur deconnexion RTC ! Initialisation des Valeurs par défaut");
        rtc.adjust(DateTime(2025, 1, 1, 0, 0, 0));
    }
    if (!bme.begin(adresseI2CduBME280)) {
        Serial.println("Erreur : BME280 introuvable !");
        displayError(SENSOR_ACCESS_ERROR);
    }
    attachInterrupt(digitalPinToInterrupt(buttonRed), handleButtonRedPress, FALLING);
    attachInterrupt(digitalPinToInterrupt(buttonGreen), handleButtonGreenPress, FALLING);
}

void loop() {
UpdateModes();
    bouton();
    Leds(currentMode);
}
