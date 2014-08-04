#include <SPI.h>
#include <LiquidCrystal.h>

// uncomment for serial monitor
// #define DEBUG 1

#define AROUND(x, y) x >= y-10 && x <= y+10
#define BUTTONDELAY 20

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);

int l = 0;
long buttonLastChecked = 0;
int lastAnalogRead = 0;
int lastAnalogReadK1 = 0;
int lastAnalogReadK2 = 0;
int lastAnalogReadK3 = 0;
int lastAnalogReadK4 = 0;
int lastAnalogReadK5 = 0;

#define STATUS_OFF 0
#define STATUS_ATTACK 1
#define STATUS_PLAY 2
#define STATUS_RELEASE 3

int status = STATUS_OFF;

bool gliding = false;
int glideLength = 0;
long glideStart = 0;
float glideIncPerMilli = 0.0;
int last_pitch = 0;
int current_pitch = 0;
int target_pitch = 0;

int envLength = 0;
long envStart = 0;
float envIncPerMilli = 0.0;

int current_cutoff = 0;
int cutoff_knob = 0;

int last_k1_display = 0;
int last_k2_display = 0;
int last_k3_display = 0;
int last_k4_display = 0;

void setup() {
#ifdef DEBUG
    Serial.begin(9600);
#endif
    pinMode(A0, INPUT);
    digitalWrite(A0, HIGH);
    pinMode(A3, INPUT);
    digitalWrite(A3, LOW);
    pinMode(A4, INPUT);
    digitalWrite(A4, LOW);
    pinMode(A5, INPUT);
    digitalWrite(A5, LOW);
    pinMode(A6, INPUT);
    pinMode(A7, INPUT);
    pinMode(3, OUTPUT);
    pinMode(10, OUTPUT);
    SPI.begin();
    lcd.begin(16, 2);
}

void loop() {

    cutoff_knob = analogRead(6) * 4;

    if(status == STATUS_ATTACK) {
        long millisPlayed = millis() - envStart;
        current_cutoff = int(envIncPerMilli * (float)millisPlayed);
        if(current_cutoff > cutoff_knob) { current_cutoff = cutoff_knob; }
#ifdef DEBUG
        Serial.print("millis=");
        Serial.print(millis());
        Serial.print(" millisPlayed=");
        Serial.print(millisPlayed);
        Serial.print(" pitch=");
        Serial.print(current_pitch);
        Serial.print(" current_cutoff=");
        Serial.println(current_cutoff);
#endif
        writeToDAC(current_pitch, current_cutoff);
        if(millisPlayed >= envLength) status = STATUS_PLAY;
    }
    if(status == STATUS_RELEASE) {
        long millisPlayed = millis() - envStart;
        current_cutoff = cutoff_knob - int(envIncPerMilli * (float)millisPlayed);
        if(current_cutoff < 0) { current_cutoff = 0; }
#ifdef DEBUG
        Serial.print("millis=");
        Serial.print(millis());
        Serial.print(" millisPlayed=");
        Serial.print(millisPlayed);
        Serial.print(" pitch=");
        Serial.print(current_pitch);
        Serial.print(" current_cutoff=");
        Serial.println(current_cutoff);
#endif
        writeToDAC(current_pitch, current_cutoff);
        if(millisPlayed >= envLength) {
            status = STATUS_OFF;
            digitalWrite(3, LOW);
            lcd.setCursor(0, 0);
            lcd.print("   ");
        }
    }

    if(status == STATUS_PLAY && gliding) {
        long millisPlayed = millis() - glideStart;
        current_pitch = last_pitch + int(glideIncPerMilli * (float)millisPlayed);
        if(glideIncPerMilli < 0.0 && current_pitch < target_pitch) {
            current_pitch = target_pitch;
            gliding = false;
        }
        if(glideIncPerMilli > 0.0 && current_pitch > target_pitch) {
            current_pitch = target_pitch;
            gliding = false;
        }
        writeToDAC(current_pitch, current_cutoff);
        if(millisPlayed > glideLength) {
            gliding = false;
        }
    }

    if( buttonLastChecked == 0 ) // see if this is the first time checking the buttons
        buttonLastChecked = millis()+BUTTONDELAY;  // force a check this cycle
    if( millis() - buttonLastChecked > BUTTONDELAY ) { // make sure a reasonable delay passed
        int b = readButtons();
        if(b != l) {
            if(b > 0) {
                digitalWrite(3, HIGH);
                envStart = millis();
                glideLength = analogRead(7);
                if(l != 0 && glideLength > 0) {
                    last_pitch = current_pitch;
                    target_pitch = noteToPitch(2, b);
                    printNote(2, b);
                    glideIncPerMilli = (float)(target_pitch-last_pitch) / (float)glideLength;
                    glideStart = millis();
                    gliding = true;
                } else {
                    current_pitch = noteToPitch(2, b);
                    printNote(2, b);
                    gliding = false;
                }
                envLength = analogRead(4);
                envIncPerMilli = (float)cutoff_knob / (float)envLength;
#ifdef DEBUG
                Serial.print("ATTACK: envStart=");
                Serial.print(envStart);
                Serial.print(" envLength=");
                Serial.print(envLength);
                Serial.print(" envIncPerMilli=");
                Serial.println(envIncPerMilli);
#endif
                current_cutoff = 0;
                if(l == 0 && envLength > 0) {
                    current_cutoff = 0;
                    status = STATUS_ATTACK;
                } else {
                    current_cutoff = cutoff_knob;
                    status = STATUS_PLAY;
                }

                writeToDAC(current_pitch, current_cutoff);
            } else {
                if(l > 0) {
                    envStart = millis();
                    envLength = analogRead(5);
                    if(envLength > 0) {
                        envIncPerMilli = (float)cutoff_knob / (float)envLength;
                        status = STATUS_RELEASE;
                    } else {
                        digitalWrite(3, LOW);
                        status = STATUS_OFF;
                        lcd.setCursor(0, 0);
                        lcd.print("   ");                        
                    }
                } else {
                    digitalWrite(3, LOW);
                    status = STATUS_OFF;
                    lcd.setCursor(0, 0);
                    lcd.print("   ");                    
                }
            }
            l = b;
        }
        buttonLastChecked = millis();
    }
    
    // TODO: don't do it every loop
    int k1 = map(analogRead(4), 0, 1023, 0, 99);
    if(k1 != last_k1_display) {
        lcd.setCursor(0, 1);
        lcd.print("A");
        writePercent(k1, 1, 1);
        last_k1_display = k1;
    }
    int k2 = map(analogRead(5), 0, 1023, 0, 99);
    if(k2 != last_k2_display) {
        lcd.setCursor(4, 1);
        lcd.print("R");
        writePercent(k2, 5, 1);
        last_k2_display = k2;
    }
    int k3 = map(analogRead(6), 0, 1023, 0, 99);
    if(k3 != last_k3_display) {
        lcd.setCursor(8, 1);
        lcd.print("C");
        writePercent(k3, 9, 1);
        last_k3_display = k3;
    }
    int k4 = map(analogRead(7), 0, 1023, 0, 99);
    if(k4 != last_k4_display) {
        lcd.setCursor(12, 1);
        lcd.print("G");
        writePercent(k4, 13, 1);
        last_k4_display = k4;
    }
        
}

int writePercent(int v, int x, int y) {
    lcd.setCursor(x, y);
    if(v < 10) {
        lcd.print(" ");
    }
    lcd.print(v);
}
    
int readButtons() {
    int a = analogRead(0);
    if(AROUND(a, 247)) {
        return 1;
    }
    if(AROUND(a, 231)) {
        return 2;
    }
    if(AROUND(a, 213)) {
        return 3;
    }
    if(AROUND(a, 195)) {
        return 4;
    }
    if(AROUND(a, 177)) {
        return 5;
    }
    if(AROUND(a, 157)) {
        return 6;
    }
    if(AROUND(a, 136)) {
        return 7;
    }
    if(AROUND(a, 114)) {
        return 8;
    }
    if(AROUND(a, 92)) {
        return 9;
    }
    if(AROUND(a, 67)) {
        return 10;
    }
    if(AROUND(a, 42)) {
        return 11;
    }
    if(AROUND(a, 15)) {
        return 12;
    }
    return 0;
}

void writeToDAC(int p, int c) {
    // write pitch to DAC1
    digitalWrite(10, LOW);
    byte data = highByte(p);
    data = 0b00001111 & data;
    data = 0b00110000 | data;
    SPI.transfer(data);
    data = lowByte(p);
    SPI.transfer(data);
    digitalWrite(10, HIGH);

    if(c == -1) return;

    // write cutoff to DAC2
    digitalWrite(10, LOW);
    data = highByte(c);
    data = 0b00001111 & data;
    data = 0b10110000 | data;
    SPI.transfer(data);
    data = lowByte(c);
    SPI.transfer(data);
    digitalWrite(10, HIGH);
}

void printNote(int octave, int note) {
    lcd.setCursor(0, 0);
    switch(note) {
        case 1: lcd.print("C"); break;
        case 2: lcd.print("C#"); break;
        case 3: lcd.print("D"); break;
        case 4: lcd.print("D#"); break;
        case 5: lcd.print("E"); break;
        case 6: lcd.print("F"); break;
        case 7: lcd.print("F#"); break;
        case 8: lcd.print("G"); break;
        case 9: lcd.print("G#"); break;
        case 10: lcd.print("A"); break;
        case 11: lcd.print("Bb"); break;
        case 12: lcd.print("B"); break;
    }
    lcd.print(octave);
    lcd.print(" ");
}
int noteToPitch(int octave, int note) {
    int pitch = 768; // C2
    pitch += (octave - 2) * 648;
    pitch += (note - 1) * 54;
    return pitch;
}
