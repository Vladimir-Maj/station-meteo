

#define DEBUG 0
#define USE_RTC 1

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#if USE_RTC
#include <RTClib.h>
  RTC_DS3231 rtc;
#endif

#if defined(__AVR_ATmega2560__)
  #define SPI_MASTER_PIN 53
#endif

const uint8_t PIN_SD_CS = 10;
const uint8_t PIN_LDR   = A0;
const uint8_t PIN_BTN_R = 2;
const uint8_t PIN_BTN_G = 4;
const uint8_t PIN_LED_A = 3;
const uint8_t PIN_LED_B = 5;

static File s_file;
static char s_active_name[16] = {0};
static uint32_t s_size = 0;

static inline void put2(char* p, uint8_t v){ p[0]='0'+(v/10); p[1]='0'+(v%10); }
static inline void yymmdd_from(uint8_t yy, uint8_t mm, uint8_t dd, char out6[6]) {
  out6[0]='0'+(yy/10); out6[1]='0'+(yy%10);
  out6[2]='0'+(mm/10); out6[3]='0'+(mm%10);
  out6[4]='0'+(dd/10); out6[5]='0'+(dd%10);
}
static inline void build_name(const char y6[6], char out[16], char rev) {
  for (uint8_t i=0;i<6;i++) out[i]=y6[i];
  out[6]='_'; out[7]=rev; out[8]='.'; out[9]='L'; out[10]='O'; out[11]='G'; out[12]='\0';
}
#if USE_RTC
static inline void ts_YmdHMS(const DateTime& t, char out[20]) {
  out[0]='2'; out[1]='0'; put2(out+2,t.year()%100); out[4]='-';
  put2(out+5,t.month()); out[7]='-'; put2(out+8,t.day()); out[10]=' ';
  put2(out+11,t.hour()); out[13]=':'; put2(out+14,t.minute()); out[16]=':'; put2(out+17,t.second()); out[19]='\0';
}
#else
static inline void ts_fake(char out[20]) {
  unsigned long s=millis()/1000UL; uint8_t hh=(s/3600UL)%24, mm=(s/60UL)%60, ss=s%60;
  out[0]='2'; out[1]='0'; out[2]='2'; out[3]='5'; out[4]='-'; out[5]='0'; out[6]='1'; out[7]='-'; out[8]='0'; out[9]='1'; out[10]=' ';
  put2(out+11,hh); out[13]=':'; put2(out+14,mm); out[16]=':'; put2(out+17,ss); out[19]='\0';
}
#endif
static inline uint8_t u16_to_str(uint16_t v, char* s) {
  char b[5]; uint8_t n=0; do{ b[n++]=char('0'+(v%10)); v/=10; }while(v); for(uint8_t i=0;i<n;i++) s[i]=b[n-1-i]; return n;
}
static inline uint8_t s16_to_str(int16_t v, char* s) {
  uint8_t n=0; uint16_t u; if (v<0){ s[n++]='-'; u=uint16_t(-v);} else u=uint16_t(v); n+=u16_to_str(u, s+n); return n;
}

static bool storage_begin(uint8_t csPin){
#if defined(__AVR_ATmega2560__)
  pinMode(SPI_MASTER_PIN, OUTPUT);
#endif
  return SD.begin(csPin);
}
static bool storage_prepare_daily(const char y6[6]){
  char n0[16]; build_name(y6,n0,'0');
  if (s_file && strncmp(s_active_name,n0,12)==0) return true;
  if (s_file) s_file.close();
  s_file = SD.open(n0, FILE_WRITE);
  if (!s_file) return false;
  s_size = s_file.size();
  for (uint8_t i=0;i<13;i++) s_active_name[i]=n0[i];
  return true;
}
static bool storage_append(const char* line, size_t len, uint32_t max_size){
  if (!s_file) return false;
  if (s_size + len > max_size){
    char y6[6]; for(uint8_t i=0;i<6;i++) y6[i]=s_active_name[i];
    char n0[16], n1[16]; build_name(y6,n0,'0'); build_name(y6,n1,'1');
    s_file.close(); SD.remove(n1);
    File src=SD.open(n0,FILE_READ), dst=SD.open(n1,FILE_WRITE);
    if (src && dst){ int c; while((c=src.read())>=0) dst.write((uint8_t)c); }
    if (src) src.close(); if (dst) dst.close();
    s_file = SD.open(n0, FILE_WRITE); if (!s_file) return false;
    s_file.seek(0); s_size=0;
  }
  size_t w = s_file.write((const uint8_t*)line, len); if (w!=len) return false;
  s_file.flush(); s_size += len; return true;
}

enum Mode : uint8_t { STANDARD=0, ECO=1, CONFIG=2, MAINT=3 };
struct ModeState { Mode current=STANDARD, pending=STANDARD; bool change=false; } M;

static inline void ledOff(){ analogWrite(PIN_LED_A,0); analogWrite(PIN_LED_B,0); }
static inline void ledStandard(){ analogWrite(PIN_LED_A,200); analogWrite(PIN_LED_B,0); }
static inline void ledEco(){ analogWrite(PIN_LED_A,0); analogWrite(PIN_LED_B,200); }
static inline void ledConfig(){ analogWrite(PIN_LED_A,200); analogWrite(PIN_LED_B,20); }
static inline void ledMaint(){ analogWrite(PIN_LED_A,150); analogWrite(PIN_LED_B,150); }
static inline void ledMode(){ switch(M.current){ case STANDARD: ledStandard(); break; case ECO: ledEco(); break; case CONFIG: ledConfig(); break; case MAINT: ledMaint(); break; } }

unsigned long t_last=0;
const uint32_t LOG_INTERVAL_MS_STD = 600000UL;
const uint32_t LOG_INTERVAL_MS_ECO = 1200000UL;
const uint32_t FILE_MAX = 4096UL;

static inline void pollButtons(){
  static unsigned long rDown=0,gDown=0;
  bool r = digitalRead(PIN_BTN_R)==LOW, g=digitalRead(PIN_BTN_G)==LOW;
  unsigned long now=millis();
  if (r && !rDown) rDown=now; if (!r) rDown=0;
  if (g && !gDown) gDown=now; if (!g) gDown=0;
  if (rDown && now-rDown>5000UL){ M.pending=(M.current==MAINT?STANDARD:MAINT); M.change=true; rDown=0; }
  if (gDown && now-gDown>5000UL){ M.pending=(M.current==ECO?STANDARD:ECO);   M.change=true; gDown=0; }
}
static inline void applyMode(){ if (!M.change) return; M.current=M.pending; M.change=false; ledMode(); }

void setup(){
  pinMode(PIN_BTN_R, INPUT_PULLUP);
  pinMode(PIN_BTN_G, INPUT_PULLUP);
  pinMode(PIN_LED_A, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  ledMode();
  Wire.begin();
#if USE_RTC
  rtc.begin();
#endif
  storage_begin(PIN_SD_CS);
  char y6[6];
#if USE_RTC
  { DateTime n=rtc.now(); yymmdd_from(n.year()%100,n.month(),n.day(),y6); }
#else
  yymmdd_from(25,1,1,y6);
#endif
  storage_prepare_daily(y6);
  t_last = millis();
}

void loop(){
  pollButtons();
  uint32_t period = (M.current==ECO)? LOG_INTERVAL_MS_ECO : LOG_INTERVAL_MS_STD;

  if (M.current==MAINT){
    ledMaint();
    t_last = millis();
    applyMode();
    delay(50);
    return;
  }
  if (M.current==CONFIG){
    ledConfig();
    t_last = millis();
    applyMode();
    delay(50);
    return;
  }

  if (millis() - t_last < period) { applyMode(); delay(50); return; }
  applyMode();
  t_last += period;

  char ts[20];
#if USE_RTC
  DateTime now = rtc.now();
  ts_YmdHMS(now, ts);
  char y6[6]; yymmdd_from(now.year()%100, now.month(), now.day(), y6);
  storage_prepare_daily(y6);
#else
  ts_fake(ts);
#endif

  uint16_t press_hPa = 1013;
  int16_t  temp_c10  = 234;
  uint8_t  hygr_pct  = 56;
  uint16_t lux_raw   = analogRead(PIN_LDR);

  char line[64]; uint8_t k=0;
  for(uint8_t i=0;i<19;i++) line[k++]=ts[i]; line[k++]=';';
  k+=u16_to_str(press_hPa, line+k); line[k++]=';';
  k+=s16_to_str(temp_c10,  line+k); line[k++]=';';
  k+=u16_to_str(hygr_pct,  line+k); line[k++]=';';
  k+=u16_to_str(lux_raw,   line+k); line[k++]='\n'; line[k]='\0';
  storage_append(line, k, FILE_MAX);
  ledMode();
}
