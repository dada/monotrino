#include <SPI.h>

const int slaveSelectPin = 10;
const int gatePin = 3;
const int downPin = 5;
const int upPin = 6;

int upState = LOW;
int last_upState = LOW;
int downState = LOW;
int last_downState = LOW;
int last_upReading = LOW;
int last_downReading = LOW;
long last_upDebounce = 0;
long last_downDebounce = 0;
long debounceDelay = 50;

int pitch = 0;
int cutoff = 0;

void setup() {
  pinMode(gatePin, OUTPUT);
  pinMode(slaveSelectPin, OUTPUT);
  SPI.begin();
  Serial.begin(9600);
}

void loop() {

  // up button
  int reading = digitalRead(upPin);
  if(reading != last_upReading) {
    last_upDebounce = millis();
    last_upReading = reading;
  }
  if(millis() - last_upDebounce > debounceDelay) {
    if(last_upState == LOW && reading == HIGH) {
      upState = HIGH;
    } else {
      upState = LOW;
    }
    last_upState = reading;
  }

  // down button  
  reading = digitalRead(downPin);
  if(reading != last_downReading) {
    last_downDebounce = millis();
    last_downReading = reading;
  }
  if(millis() - last_downDebounce > debounceDelay) {
    if(last_downState == LOW && reading == HIGH) {
      downState = HIGH;
    } else {
      downState = LOW;
    }
    last_downState = reading;
  }
    
  // adjust pitch
  if(upState == HIGH) {
    pitch += 10;
    if(pitch > 4000) { pitch = 4000; }
  }
  if(downState == HIGH) {
    pitch -= 10;
    if(pitch < 0) { pitch = 0; } 
  }

  // fine tune (knob)
  int d = map(analogRead(0), 0, 1023, 0, 9);
  
  int pitch_d = pitch + d;
  Serial.print("pitch=");
  Serial.println(pitch_d);
  cutoff = 2000;

  digitalWrite(gatePin, HIGH); 
  writeToDAC(pitch_d, cutoff);

}

void writeToDAC(int p, int c) {  
  // write pitch to DAC channel 1
  digitalWrite(slaveSelectPin, LOW);
  byte data = highByte(p);
  data = 0b00001111 & data;
  data = 0b00110000 | data;
  SPI.transfer(data);
  data = lowByte(p);
  SPI.transfer(data);
  digitalWrite(slaveSelectPin, HIGH);

  // write cutoff to DAC channel 2
  digitalWrite(slaveSelectPin, LOW);
  data = highByte(c);
  data = 0b00001111 & data;
  data = 0b10110000 | data;
  SPI.transfer(data);
  data = lowByte(c);
  SPI.transfer(data);  
  digitalWrite(slaveSelectPin, HIGH);
}



