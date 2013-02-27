#include <SPI.h>

const int slaveSelectPin = 10;
const int gatePin = 3;

int incr = 0;
int decr = 0;
int pitch = 0;
int cutoff = 0;
int attack = 0;
int release = 0;
long previousMillis = 0;
int playingMillis = 0;

int pattern[32] = {
  1382, 1382, 1436, 1544, 1544, 1436, 1382, 1274,
  1166, 1166, 1274, 1382, 1382, 1274, 1274, 0,
  1382, 1382, 1436, 1544, 1544, 1436, 1382, 1274,
  1166, 1166, 1274, 1382, 1274, 1166, 1166, 0
};

int step = 0;

void setup() {
  pinMode(gatePin, OUTPUT);
  pinMode(slaveSelectPin, OUTPUT);  
  SPI.begin();
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long t = currentMillis - previousMillis;
  playingMillis = (int) t;
  if(playingMillis > 500) {
    previousMillis = currentMillis;
    pitch = pattern[step];
    if(pitch == 0) {
      digitalWrite(gatePin, LOW);
    } else {
      digitalWrite(gatePin, HIGH);
    }
      
    attack = 200;
    release = 200;
    cutoff = 0;
      
    incr = (4000-cutoff)/attack;
    decr = 4000/release;
    step++;
    if(step == 32) step = 0;
  }
  
  if(playingMillis < attack) {
    cutoff += incr;
  }
  if(playingMillis > (300-release)) {
    cutoff -= decr;
  }
  if(cutoff > 4000) cutoff = 4000;
  if(cutoff < 1) cutoff = 1;
  
  writeToDAC(pitch, cutoff);
}

void writeToDAC(int p, int c) {  
  // write pitch to DAC1
  digitalWrite(slaveSelectPin, LOW);
  byte data = highByte(p);
  data = 0b00001111 & data;
  data = 0b00110000 | data;
  SPI.transfer(data);
  data = lowByte(p);
  SPI.transfer(data);
  digitalWrite(slaveSelectPin, HIGH);

  // write cutoff to DAC2
  digitalWrite(slaveSelectPin, LOW);
  data = highByte(c);
  data = 0b00001111 & data;
  data = 0b10110000 | data;
  SPI.transfer(data);
  data = lowByte(c);
  SPI.transfer(data);  
  digitalWrite(slaveSelectPin, HIGH);
}



