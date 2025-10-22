#include <Arduino.h>

//paramètre par défaut

//temps d'attente si capteur ne répond pas
#define TIMEOUT 30

//paramètre capteur luminosité
#define LUMIN 1
#define LUMIN_LOW 200
#define LUMIN_HIGH 700

//parapètre capteur température, humidité et pression
#define TEMP_AIR 1
#define MIN_TEMP_AIR -10
#define MAX_TEMP_AIR 40

#define HYGR 1
#define HYGR_MINT 0
#define HYGR_MAXT 50

#define PRESSURE 1
#define PRESSURE_MIN 850
#define PRESSURE_MAX 1080

//interval entre 2 mesures
#define LOG_INTERVAL 10 //min

//taille max fichier pour fichier 
#define FILE_MAX_SIZE 4096//octets

#define NUM_LEDS 1

//mode
enum Mode {CONFIG,STAND,ECO,MAINT};
Mode currentMode = Mode::STAND;

constexpr uint8_t BUTROUGE = 3;
constexpr uint8_t BUTVERT  = 2;
constexpr uint8_t LEDL     = 8;

constexpr uint16_t timeBut = 5000;// 5sec lorsqu'on appuis
constexpr uint16_t antiparasite = 50; // pour eviter les parasites lorsqu'on appuit sur le boutton

//changement de mode via les boutton
void updateMode() {
  static bool redDown=false, greenDown=false;
  static bool redTrig=false, greenTrig=false;       // déclenché à 5s ?
  static uint32_t tRed=0, tGreen=0;
  static uint32_t lastAnnRed=0, lastAnnGreen=0;     // annonces 1 Hz
  static uint32_t lastEdgeRed=0, lastEdgeGreen=0;   // anti-rebond

  const uint32_t now = millis();

  // -------- ROUGE --------
  bool r = (digitalRead(BUTROUGE) == LOW);          // INPUT_PULLUP
  if (r && !redDown && (now - lastEdgeRed >= antiparasite)) {
    redDown = true; redTrig = false;
    tRed = now; lastAnnRed = now; lastEdgeRed = now;
    Serial.println(F("[Rouge] appui détecté : maintenir 5s..."));
  } else if (!r && redDown && (now - lastEdgeRed >= antiparasite)) {
    redDown = false; redTrig = false; lastEdgeRed = now;
    if (now - tRed < timeBut) Serial.println(F("[Rouge] appui <5s, aucun changement."));
  }
  if (redDown && !redTrig) {
    if (now - lastAnnRed >= 1000) {
      Serial.print(F("[Rouge] ")); Serial.print((now - tRed)/1000);
      Serial.println(F("s / 5s"));
      lastAnnRed = now;
    }
    if ((now - tRed) >= timeBut) {
      redTrig = true;
      // Transitions demandées côté ROUGE
      if (currentMode == Mode::STAND) {
        currentMode = Mode::MAINT;       Serial.println(F("→ Changement de mode: MAINTENANCE"));
      } else if (currentMode == Mode::MAINT) {
        currentMode = Mode::STAND;       Serial.println(F("→ Changement de mode: STANDARD"));
      } else if (currentMode == Mode::CONFIG) {
        currentMode = Mode::STAND;       Serial.println(F("→ Changement de mode: STANDARD"));
      } else {
        Serial.println(F("[Rouge] long: aucun changement dans ce mode."));
      }
    }
  }

  // -------- VERT --------
  bool g = (digitalRead(BUTVERT) == LOW);
  if (g && !greenDown && (now - lastEdgeGreen >= antiparasite)) {
    greenDown = true; greenTrig = false;
    tGreen = now; lastAnnGreen = now; lastEdgeGreen = now;
    Serial.println(F("[Vert] appui détecté : maintenir 5s..."));
  } else if (!g && greenDown && (now - lastEdgeGreen >= antiparasite)) {
    greenDown = false; greenTrig = false; lastEdgeGreen = now;
    if (now - tGreen < timeBut) Serial.println(F("[Vert] appui <5s, aucun changement."));
  }
  if (greenDown && !greenTrig) {
    if (now - lastAnnGreen >= 1000) {
      Serial.print(F("[Vert] ")); Serial.print((now - tGreen)/1000);
      Serial.println(F("s / 5s"));
      lastAnnGreen = now;
    }
    if ((now - tGreen) >= timeBut) {
      greenTrig = true;
      // Transitions demandées côté VERT
      if (currentMode == Mode::STAND) {
        currentMode = Mode::ECO;         Serial.println(F("→ Changement de mode: ÉCONOMIE"));
      } else if (currentMode == Mode::ECO) {
        currentMode = Mode::STAND;       Serial.println(F("→ Changement de mode: STANDARD"));
      } else {
        Serial.println(F("[Vert] long: aucun changement dans ce mode."));
      }
    }
  }
}



void setup() {
    Serial.begin(9600);

  pinMode(BUTROUGE, INPUT_PULLUP);
  pinMode(BUTVERT,  INPUT_PULLUP);
  currentMode = (digitalRead(BUTROUGE) == LOW) ? Mode::CONFIG : Mode::STAND;
 


}

void loop() {

updateMode();
}
