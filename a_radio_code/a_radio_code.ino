#include <Wire.h>
#include <radio.h>
#include <RDA5807M.h>
#include <RDSParser.h>

RDA5807M radio;
RDSParser rds;

#include <TFT.h>
#include <SPI.h>

// create an instance of the library (cs,dc,reset)
TFT screen = TFT(10, 9, A0);

#define VCCMIN   3780
#define VCCMAX   4150
#define PRESETS  11
#define STOREAGE 16
#define SERIAL_SPEED  9600 // unused

const int pin_NEXT = 8; // <--------------- next station

const int pin_A     = A1;  // <------------- volume +
const int pin_B     = A2;  // <------------- volume -
const int pin_AUDIO = A3;

int val  = 3;
int dati = 0;
char buf1Printout[6];
bool changed=true;
bool powersave=false;

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

static char* presetName[PRESETS] = {
  "Welle NR",
  "WDR 5",
  "News 89.4",
  "WDR 3",
  "D Kultur",
  "WDR 2",
  "WDR 4",
  "DLF",
  "Funkhaus",
  "BFBS",
  "1 live"
};


RADIO_INFO rinfo;
int rssiold=0;
int vcc = VCCMIN;
int vccold = vcc;
char namestr[] = "           ";

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
  if (vccold != vcc) {
    vccold = vcc;
    changed = true;    
  }
}

void DisplayServiceName(char *name) {
  sprintf(namestr, "%s", name);
  changed=true;
}

struct Midways {
  byte _val[STOREAGE];
  int  _nxt;
  byte _max;
  byte _min;

  Midways() {
    _nxt = 0;
    for (int i=0; i<STOREAGE; ++i) { 
      _val[i] = 128;
    }
  }

  void add(byte val) {
    _val[_nxt] = val;
    _nxt++;
    if (_nxt == STOREAGE) {
      _nxt = 0;
    }
  }

  byte last() {
    int l = _nxt -1;
    if (l < 0) l += STOREAGE;
    return _val[l];
  }

  byte midget() {
    int mid = 0;
    _min = 255;
    _max = 0;
    for (int i=0; i<STOREAGE; ++i) {
      if (_val[i] > _max) _max=_val[i];
      if (_val[i] < _min) _min=_val[i];
      mid += _val[i];
    }
    
    return (mid/STOREAGE);
  }

  void draw(int x, int y) {
    int id = _nxt-1;
    byte mid = midget();
    
    byte dx = x + STOREAGE;
    short dy = y - (_val[id] - mid);
    
    if (id < 0) id += STOREAGE;
    for (int i=0; i<STOREAGE; ++i) {
      
      dx = x+ 9*(STOREAGE-i);
      dy = y - (_val[id] - mid);
      if (dy < 44) dy = 44;
      if (dy > 84) dy = 84;
      screen.stroke(0,200,0);
      screen.line(dx, 84, dx, dy); 
      if (dy<60) {
        screen.stroke(255,64,0);
        screen.line(dx, 60, dx, dy); 
      }
      screen.stroke(0,0,0); 
      screen.line(dx, dy, dx, 44); 
      screen.stroke(255,255,255);
      screen.point(dx, dy);
      id--;
      if (id < 0) id += STOREAGE;
    }
  }
} daten;


void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
}

void setup() {
  Serial.begin(SERIAL_SPEED);
  pinMode(pin_NEXT, INPUT_PULLUP);
  pinMode(pin_A, INPUT_PULLUP);
  pinMode(pin_B, INPUT_PULLUP);
  pinMode(pin_AUDIO, INPUT);
    
  screen.begin();
  screen.setTextSize(1);
    
  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, preset[idx]);
  DisplayServiceName(presetName[idx]);

  screen.background(0, 0, 0);
  screen.stroke(255, 255, 255);

  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(val); 
  radio.attachReceiveRDS(RDS_process);
  rds.attachServicenNameCallback(DisplayServiceName);
}

void loop() { 
  readVcc();
  radio.getRadioInfo(&rinfo);
  radio.checkRDS();

  if(rssiold != rinfo.rssi) {
    rssiold = rinfo.rssi;
    changed=true;
  }
  
  if (digitalRead(pin_NEXT) == LOW) {
    if (idx==PRESETS-1) idx=0; else idx++;
    radio.setFrequency(preset[idx]);
    DisplayServiceName(presetName[idx]);
    changed=true;
    delay(200);
  }
  if (digitalRead(pin_A) == LOW) {
    if (val<1) {
      radio.setMute(false);
      powersave=false;
    }
    if (val>14) val=13;
    radio.setVolume(++val);
    changed=true;
    delay(50);
  }
  if (digitalRead(pin_B) == LOW) {
    if (val<1) {
      radio.setMute(true);
      powersave=true;
      screen.background(0, 0, 0);
      delay(200);
    } else {
      radio.setVolume(--val);
      changed=true;
    }
    delay(50);
  }

  dati = analogRead(pin_AUDIO);
  daten.add(dati>>(val/5));

  if (changed == true && powersave==false) {
    changed=false;
    
    // clear areas
    screen.fill(0,0,0);
    screen.stroke(0,0,0);
    
    screen.rect(0, 30, 13, 8);
    screen.rect(30, 30, 85, 3);
    screen.rect(150, 28, 10, 10);  
    screen.rect(0, 88, 159, 10);

    // display power
    screen.noFill();
    screen.stroke(255,255,255);
    screen.rect(0, 28, 17, 12);
    screen.rect(17, 32, 2, 4);
    screen.fill(0,255,0);
    screen.rect(2, 30, map(vcc, VCCMIN, VCCMAX, 1, 13), 8);

    // display signal
    screen.stroke(64,128,255);
    if (rinfo.rssi > 4)  screen.line(151, 38, 151, 36);
    if (rinfo.rssi > 10) screen.line(153, 38, 153, 34);
    if (rinfo.rssi > 22) screen.line(155, 38, 155, 32);
    if (rinfo.rssi > 28) screen.line(157, 38, 157, 30);
    if (rinfo.rssi > 32) screen.line(159, 38, 159, 28);

    // display radio station
    screen.stroke(255,255,255);
    screen.text(namestr, 0, 90);

    // display radio freq
    String buf1 = String(((float)radio.getFrequency())/100.0);
    buf1.toCharArray(buf1Printout, 6);
    screen.text(buf1Printout, 124, 90); 

    // display volume
    screen.stroke(128,255,255);
    screen.line(78, 30, 77  -2*val, 32);
    screen.line(78, 30, 77  -2*val, 31);
    screen.line(78, 30, 77  -2*val, 30);  
    screen.line(82, 30, 83  +2*val, 32);
    screen.line(82, 30, 83  +2*val, 31);
    screen.line(82, 30, 83  +2*val, 30);
  }

  if (powersave==false) {
    // display audio
    daten.draw(4, 65);
  }
}

