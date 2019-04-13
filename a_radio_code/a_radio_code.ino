#include <Wire.h>
#include <radio.h>
#include <RDA5807M.h>
#include <RDSParser.h>

RDA5807M radio;
RDSParser rds;

#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
Adafruit_SSD1306 oled(-1);

#define VCCMIN   3650
#define VCCMAX   4150
#define PRESETS  11

const int pin_UP   = A2; // <--------------- low to count preset station up
const int pin_DOWN = A3; // <--------------- low to count preset station down

const int pin_A      = 3;  // <--------------- rotation encoder (tuning)
const int pin_B      = 4;  // <--------------- rotation encoder (tuning)

int sensorLR         = A0; // <-------------- 25k Poti for volume
int val = 650;

bool logg[8] = {0,0,0,0, 0,0,0,0};
int tick = 0;
bool changes = false;
uint16_t potidelay = 0;

int valA = 0;
int valB = 0;
int valA2 = 0;
int valB2 = 0;

int idx = 0;
RADIO_FREQ preset[PRESETS] = {
  8770,
  8880,
  8940,
  9510,
  9650,
  9920,
  10130,
  10280,
  10330,
  10510,
  10670
};

RADIO_INFO rinfo;
int  vcc;
char timestr[6];
char namestr[] = "the dummy !";

void readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(5);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
  vcc = ADCL; 
  vcc |= ADCH<<8; 
  vcc = 1126400L / vcc;
  if (vcc>VCCMAX) vcc = VCCMAX;
  if (vcc<VCCMIN) vcc = VCCMIN;
}

void DisplayServiceName(char *name) {
  sprintf(namestr, "%s", name);
}

void DisplayTime(uint8_t hour, uint8_t minute) {
  sprintf(timestr, "%02d:%02d", hour, minute);
}

void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
}

void setup() {  
  pinMode(pin_UP,   INPUT_PULLUP);
  pinMode(pin_DOWN, INPUT_PULLUP);
  pinMode(pin_A, INPUT_PULLUP);
  pinMode(pin_B, INPUT_PULLUP);
    
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.display();
    
  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, preset[idx]);
  delay(100);
  radio.setMono(false);
  radio.setMute(false);
  radio.attachReceiveRDS(RDS_process);
  rds.attachServicenNameCallback(DisplayServiceName);
  rds.attachTimeCallback(DisplayTime);
}

void loop() {
  radio.checkRDS();
  potidelay++;
  changes = false;
  if (potidelay%2048 == 0) {    
    int vol = radio.getVolume();
    val = 15 - (val/64);
    if (vol != val) radio.setVolume(val);

    oled.clearDisplay();
    
    // display power
    readVcc();
    oled.drawRect(0, 0, 17, 12, WHITE);
    oled.drawRect(17, 4, 2, 4, WHITE);
    oled.fillRect(2, 2, map(vcc, VCCMIN, VCCMAX, 1, 13), 8, WHITE);

    // display time
    oled.setCursor(50,0);
    oled.print(timestr);
    
    // display signal
    radio.getRadioInfo(&rinfo);    
    if (rinfo.rssi > 4) oled.drawLine(119, 10, 119, 8, WHITE);
    if (rinfo.rssi > 10) oled.drawLine(121, 10, 121, 6, WHITE);
    if (rinfo.rssi > 22) oled.drawLine(123, 10, 123, 4, WHITE);
    if (rinfo.rssi > 28) oled.drawLine(125, 10, 125, 2, WHITE);
    if (rinfo.rssi > 32) oled.drawLine(127, 10, 127, 0, WHITE);

    // display volume
    oled.drawLine(62, 10, 61  -2*val, 12, WHITE);
    oled.drawLine(62, 10, 61  -2*val, 11, WHITE);
    oled.drawLine(62, 10, 61  -2*val, 10, WHITE);

    oled.drawLine(66, 10, 67  +2*val, 12, WHITE);
    oled.drawLine(66, 10, 67  +2*val, 11, WHITE);
    oled.drawLine(66, 10, 67  +2*val, 10, WHITE);

    // display radio station
    oled.setCursor(0,24);
    oled.print(namestr);

    // display radio freq
    oled.setCursor(90,24);
    oled.print(((float)radio.getFrequency())/100.0);
    oled.display();    
  }
  
  val  = analogRead(sensorLR);
  valA = digitalRead(pin_A);
  valB = digitalRead(pin_B);
  
  if (digitalRead(pin_UP) == LOW) {
    if (idx==PRESETS-1) idx=0; else idx++;
    radio.setFrequency(preset[idx]);
    delay(200);
  }
  if (digitalRead(pin_DOWN) == LOW) {
    if (idx==0) idx=PRESETS-1; else idx--;
    radio.setFrequency(preset[idx]);
    delay(200);
  }

  if (valA != valA2 || valB != valB2) {
    if (valA && valB) {
      tick=0;
      for (int i=0;i<8;++i) {
        logg[i] = false;
      }
    }
    logg[tick] = valA;
    tick = (tick+1)%8;
    logg[tick] = valB;
    tick = (tick+1)%8;
    valA2 = valA;
    valB2 = valB;
    changes = true;
  }
  if (changes) {
    if (
      logg[0] == 1 &&
      logg[1] == 1 &&
      logg[2] == 0 &&
      logg[3] == 1 &&
      logg[4] == 0 &&
      logg[5] == 0 &&
      logg[6] == 1 &&
      logg[7] == 0
    ) {
      radio.seekDown(false);   
    }
    if (
      logg[0] == 1 &&
      logg[1] == 1 &&
      logg[2] == 1 &&
      logg[3] == 0 &&
      logg[4] == 0 &&
      logg[5] == 0 &&
      logg[6] == 0 &&
      logg[7] == 1
    ) {
      radio.seekUp(false);      
    }    
  }    
}

