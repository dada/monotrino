#include <SPI.h>
#include <LiquidCrystal.h>

// uncomment for serial monitor
// #define DEBUG 1

#define AROUND(x, y) x >= y-10 && x <= y+10
#define BUTTONDELAY 20

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);

#define BUTTON_PLAY 1
#define BUTTON_MENU 2
#define BUTTON_OCTUP 3
#define BUTTON_OCTDN 4
#define BUTTON_REST 5
#define BUTTON_LEFT 6
#define BUTTON_RIGHT 7
#define BUTTON_ENTER 8

long button_note_last_checked = BUTTONDELAY;
long button_cmd_last_checked = BUTTONDELAY;
long button_patt_last_checked = BUTTONDELAY;
byte button_cmd = 0;
byte button_cmd_last = 0;
byte button_note = 0;
byte button_note_last = 0;
byte button_patt = 0;
byte button_patt_last = 0;
int lastAnalogRead = 0;
int lastAnalogReadK1 = 0;
int lastAnalogReadK2 = 0;
int lastAnalogReadK3 = 0;
int lastAnalogReadK4 = 0;
int lastAnalogReadK5 = 0;

#define MODE_SPLASH 0
#define MODE_SEQ_PLAY 1
#define MODE_SEQ_INPUT 2
#define MODE_SEQ_EDIT 3
#define MODE_PIANO 4
#define MODE_MENU 128
byte mode = MODE_SPLASH;
byte mode_last = MODE_SEQ_EDIT;

#define STATUS_OFF 0
#define STATUS_ATTACK 1
#define STATUS_PLAY 2
#define STATUS_RELEASE 3
#define STATUS_PREVIEW_NOTE 4
#define STATUS_ENVMOD_ATTACK 5
#define STATUS_HOLD 6
#define STATUS_DECLICK 7
byte status = STATUS_OFF;
byte env_status = STATUS_OFF;

byte current_step = 0;
byte current_octave = 2;
long millis_step_start = 0;
long millis_per_step = 15000/120;
int pattern[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
// On the run - Pink Floyd (C2  Eb2 G2  F2  Eb2 F2  Bb2 C3)
// int pattern[16] = { 25, 28, 32, 30, 28, 30, 35, 37, 25, 28, 32, 30, 28, 30, 35, 37 };
byte pattern_cutoff[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
byte pattern_envmod[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
byte pattern_decay[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
byte pattern_glide[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int selected_step = 0;
int modified_pitch;
byte modified_cutoff = 0;
byte modified_envmod = 0;
byte modified_decay = 0;
byte modified_glide = 0;
int bpm = 0;

bool gliding = false;
int glide_length = 0;
long glide_start = 0;
float glide_inc_per_milli = 0.0;
int last_pitch = 0;
int current_pitch = 0;
int target_pitch = 0;

int env_length = 0;
long env_start = 0;
float env_inc_per_milli = 0.0;

int current_cutoff = 0;
int cutoff_knob = 0;
int cutoff_min = 0;
int cutoff_max = 1024;

int last_k1_display = 0;
int last_k2_display = 0;
int last_k3_display = 0;
int last_k4_display = 0;

long printed_step_data = 0;

#define CHAR_STEP_EMPTY byte(0)
#define CHAR_STEP_EMPTY_Q 1
#define CHAR_STEP_FULL 2
#define CHAR_STEP_FULL_Q 3
#define CHAR_STEP_HOLD 4
#define CHAR_STEP_HOLD_Q 5
#define CHAR_STEP_SLIDE 6
#define CHAR_STEP_SLIDE_Q 7

float cutoff_zero;
float cutoff_top;
int hold_time;
int attack_time;
int release_time;
int current_envmod;
int current_decay;

void setup() {
#ifdef DEBUG
    Serial.begin(9600);
#endif
    pinMode(A0, INPUT);
    digitalWrite(A0, HIGH);
    pinMode(A1, INPUT);
    digitalWrite(A1, HIGH);
/*
    // pattern buttons
    pinMode(A2, INPUT);
    digitalWrite(A2, HIGH);
*/
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

    /* setup splash */
    env_start = millis();
    lcd.setCursor(0, 0);
    lcd.print("mono/trino v0.01");
    lcd.setCursor(0, 1);
    lcd.print("  by dada 2014  ");
}

void loop() {
    loop_common();
    switch(mode) {
    case MODE_SPLASH:
        loop_splash();
        break;
    case MODE_SEQ_PLAY:
        loop_seq_play();
        break;
    case MODE_SEQ_INPUT:
        loop_seq_input();
        break;
    case MODE_SEQ_EDIT:
        loop_seq_edit();
        break;
    case MODE_PIANO:
        loop_piano();
        break;
    case MODE_MENU:
        loop_menu();
        break;
    }
}

void loop_splash() {
    if(millis() - env_start > 2000) {
        // init tempo
        int t = analogRead(3);
        if(t < 512) {
            bpm = map(t, 0, 512, 30, 120);
        } else {
            bpm = map(t, 512, 1023, 120, 300);
        }
        millis_per_step = (int)(15000.0/(float)bpm );
        initMode(MODE_SEQ_INPUT);
    }
}

void initMode(byte m) {
    if(m != MODE_MENU) {
        mode_last = m;
    }
    mode = m;
    switch(mode) {
    case MODE_SEQ_PLAY:
        createSeqChars();
        printPattern();
        lcd.setCursor(0, 1);
        lcd.print("p1 --");
        lcd.print(current_octave);
        lcd.print("      ");
        printTempo();
        lcd.cursor();
        lcd.blink();
        break;
    case MODE_SEQ_EDIT:
        createSeqChars();
        printPattern();
        lcd.setCursor(0, 1);
        lcd.print("e1 --");
        lcd.print(current_octave);
        lcd.print("      ");
        printTempo();
        selected_step = 0;
        printStep(selected_step);
        lcd.setCursor(selected_step, 0);
        lcd.cursor();
        lcd.blink();
        break;
    case MODE_SEQ_INPUT:
        createSeqChars();
        printPattern();
        lcd.setCursor(0, 1);
        lcd.print("i1 --");
        lcd.print(current_octave);
        lcd.print("      ");
        printTempo();
        selected_step = 0;
        lcd.setCursor(selected_step, 0);
        lcd.cursor();
        lcd.blink();
        break;
    case MODE_MENU:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("not implemented");
        lcd.noCursor();
        lcd.noBlink();
        break;
    }
}

void loop_common() {
    if( millis() - button_cmd_last_checked > BUTTONDELAY ) {
        button_cmd = readCmdButtons();
        if(button_cmd == button_cmd_last) {
            button_cmd = 0;
        } else {
            button_cmd_last = button_cmd;
        }
        button_cmd_last_checked = millis();
#if DEBUG
        Serial.print("button_cmd=");
        Serial.print(button_cmd);
        Serial.print(" button_cmd_last=");
        Serial.print(button_cmd_last);
        Serial.println(".");
#endif
    }
    if( millis() - button_note_last_checked > BUTTONDELAY ) {
        button_note = readNoteButtons();
        if(mode != MODE_PIANO) {
            if(button_note == button_note_last) {
                button_note = 0;
            } else {
                button_note_last = button_note;
            }
        }
        button_note_last_checked = millis();
    }
/*
    if( millis() - button_patt_last_checked > BUTTONDELAY ) {
        button_patt = readPattButtons();
        if(button_patt == button_patt_last) {
            button_patt = 0;
        } else {
            button_patt_last = button_patt;
        }
        button_patt_last_checked = millis();
    }
*/
    if(button_cmd != 0) {
        switch(button_cmd) {
        case BUTTON_MENU:
            if(mode == MODE_MENU) {
                initMode(mode_last);
            } else {
                initMode(MODE_MENU);
            }
            button_cmd = 0;
            break;
        }
    }
#if DEBUG
    if(button_cmd != 0) {
        Serial.print("mode=");
        Serial.print(mode);
        Serial.print(" status=");
        Serial.print(status);
        Serial.print(" button_cmd=");
        Serial.print(button_cmd);
        Serial.print(" button_cmd_last=");
        Serial.print(button_cmd_last);
        Serial.print(" button_note=");
        Serial.print(button_note);
        Serial.println(".");
    }
#endif
}

void loop_piano() {
    // see monotrino_piano.ino
}

void loop_seq_common() {
    current_cutoff = map(analogRead(6), 0, 1023, cutoff_min, cutoff_max);
    // read tempo pot
    int b;
    int t = analogRead(3);
    if(t < 512) {
        b = map(t, 0, 512, 30, 120);
    } else {
        b = map(t, 512, 1023, 120, 300);
    }
    if(b != bpm) {
        bpm = b;
        printTempo();
        millis_per_step = (int)(15000.0/(float)bpm );
#if DEBUG
        Serial.print("millis_per_step=");
        Serial.println(millis_per_step);
#endif
    }
    if(button_cmd == BUTTON_PLAY) {
        if(mode == MODE_SEQ_INPUT) {
            mode = MODE_SET_EDIT;
            initMode(mode);
        }
        if(status == STATUS_PLAY) {
#if DEBUG
            Serial.println("STOPPING");
#endif
            status = STATUS_OFF;
            digitalWrite(3, LOW);
        } else {
#if DEBUG
            Serial.println("PLAYING");
#endif
            status = STATUS_PLAY;
            writeToDAC(noteNumberToPitch(pattern[current_step]), current_cutoff);
            digitalWrite(3, HIGH);
            millis_step_start = millis();
        }
        button_cmd = 0;
    }
    if(status == STATUS_PLAY || status == STATUS_PREVIEW_NOTE) {
        unsigned long millis_now = millis();
        if(millis_now - millis_step_start > millis_per_step) {
#if DEBUG
            Serial.print("NEXT STEP millis_step_start=");
            Serial.print(millis_step_start);
            Serial.print(" millis_now=");
            Serial.print(millis_now);
            Serial.print(" millis_per_step=");
            Serial.println(millis_per_step);
#endif
            initStep();
            millis_step_start = millis_now;
            if(status == STATUS_PLAY) {
                nextStep();
            } else {
                status = STATUS_OFF;
                digitalWrite(3, LOW);
            }
        } else {
            unsigned long millis_played = millis_now - env_start;
            switch(env_status) {
            case STATUS_ATTACK:
                current_cutoff = int(cutoff_zero + env_inc_per_milli * (float)millis_played);
                if(current_cutoff > (int)cutoff_top) { current_cutoff = (int)cutoff_top; }
                writeToDAC(noteNumberToPitch(pattern[current_step]), current_cutoff);
                if(millis_played >= env_length) {
                    if(hold_time > 0) {
                        env_status = STATUS_HOLD;
                        env_start = millis_now;
                        env_length = (int)hold_time;
                    } else {
                        env_status = STATUS_RELEASE;
                        env_start = millis_now;
                        env_length = release_time;
                        current_cutoff = (int)cutoff_top;
                        env_inc_per_milli = cutoff_top  / (float)env_length;
                    }
                }
                break;
            case STATUS_HOLD:
                if(millis_played >= env_length) {
                    env_status = STATUS_RELEASE;
                    env_start = millis_now;
                    env_length = release_time;
                    current_cutoff = (int)cutoff_top;
                    env_inc_per_milli = cutoff_top / (float)env_length;
                }
                break;
            case STATUS_RELEASE:
                current_cutoff = int(cutoff_top - env_inc_per_milli * (float)millis_played);
                if(current_cutoff <= 0) {
                    env_status = STATUS_OFF;
                    digitalWrite(3, LOW);
                } else {
                    writeToDAC(noteNumberToPitch(pattern[current_step]), current_cutoff);
                }
                if(millis_played >= env_length) {
                    env_status = STATUS_OFF;
                    digitalWrite(3, LOW);
                }
                break;
            }
        }
    }
}

void initStep() {
    int cutoff = map(analogRead(6), 0, 1023, cutoff_min, cutoff_max);
    int ck = map(cutoff, cutoff_min, cutoff_max, 0, 100);
    if(ck != cutoff_knob) {
        printKnob('C', ck);
        cutoff_knob = ck;
    }
    if(pattern_cutoff[current_step] != 0) {
        float c = (float)cutoff * ((pattern_cutoff[current_step]-127.0)/256.0 + 1.0);
        cutoff = (int) c;
        if(cutoff > cutoff_max) cutoff = cutoff_max;
        if(cutoff < cutoff_min) cutoff = cutoff_min;
    }
/*
    int cutoff = map(analogRead(6), 0, 1023, cutoff_min, cutoff_max);
    if(current_cutoff != cutoff) {
        printKnob('C', cutoff);
        current_cutoff = cutoff;
    }
*/
    int envmod = map(analogRead(4), 0, 1023, 0, 80);
    if(current_envmod != envmod) {
        printKnob('E', envmod);
        current_envmod = envmod;
    }
    if(pattern_envmod[current_step] != 0) {
        float c = (float)current_envmod * ((pattern_envmod[current_step]-127.0)/256.0 + 1.0);
        current_envmod = (int) c;
        if(current_envmod > 80) current_envmod = 80;
        if(current_envmod < 0) current_envmod = 0;
    }
    int decay = map(analogRead(5), 0, 1023, 20, 300);
    if(current_decay != decay) {
        printKnob('D', decay);
        current_decay = decay;
    }
    if(pattern_decay[current_step] != 0) {
        float c = (float)current_decay * ((pattern_decay[current_step]-127.0)/256.0 + 1.0);
        current_decay = (int) c;
        if(current_decay > 300) current_envmod = 300;
        if(current_envmod < 20) current_envmod = 20;
    }
    cutoff_zero = (float)cutoff * (100.0 - (float)current_envmod / 100.0);
    // cutoff_zero = (float)current_cutoff;
    cutoff_top = (float)cutoff + ((float)cutoff_max - (float)cutoff) * (float)current_envmod / 100.0;
    int total_length = current_decay;
    hold_time = (int)( (float)total_length * (100.0 - (float)current_envmod) / 100.0 );
    attack_time = 10;
    release_time = total_length - hold_time - 10;
    if(release_time < 0) release_time = 0;
#ifdef DEBUG
    lcd.setCursor(0, 1);
    lcd.print((int)total_length);
    lcd.print(":");
    lcd.print(attack_time);
    lcd.print(">");
    lcd.print(hold_time);
    lcd.print(">");
    lcd.print(release_time);
#endif
    env_status = STATUS_ATTACK;
    env_length = attack_time;
    // if(envmod == 0) {
        // current_cutoff = (int) cutoff_zero;
    // }
    // env_inc_per_milli = (cutoff_top - cutoff_zero) / (float)env_length;
    // env_inc_per_milli = (cutoff_zero - current_cutoff) / (float)env_length;
    env_inc_per_milli = (cutoff_top - (float)current_cutoff) / (float)env_length;
    env_start = millis();
}

void loop_seq_play() {
    loop_seq_common();
}

void loop_seq_input() {
    loop_seq_common();
    switch(button_cmd) {
    case BUTTON_OCTUP:
        current_octave++;
        if(current_octave > 4) current_octave = 4;
        lcd.setCursor(3, 1);
        lcd.print("--");
        lcd.print(current_octave);
        lcd.setCursor(current_step, 0);
        button_cmd = 0;
        break;
    case BUTTON_OCTDN:
        current_octave--;
        if(current_octave < 1) current_octave = 1;
        lcd.setCursor(3, 1);
        lcd.print("--");
        lcd.print(current_octave);
        lcd.setCursor(current_step, 0);
        button_cmd = 0;
        break;
    case BUTTON_REST:
        pattern[current_step] = 0;
        lcd.setCursor(current_step, 0);
        lcd.write(current_step % 4 == 0 ? CHAR_STEP_EMPTY_Q : CHAR_STEP_EMPTY);
        lcd.setCursor(3, 1);
        lcd.print("--");
        lcd.print(current_octave);
        current_step++;
        lcd.setCursor(current_step, 0);
        button_cmd = 0;
        break;
    }
    if(button_note > 0) {
        status = STATUS_PREVIEW_NOTE;
        pattern[current_step] = noteNumber(current_octave, button_note);
        printNote(current_octave, button_note);
        writeToDAC(noteNumberToPitch(pattern[current_step]), current_cutoff);
        digitalWrite(3, HIGH);
        lcd.setCursor(current_step, 0);
        lcd.write(current_step % 4 == 0 ? CHAR_STEP_FULL_Q : CHAR_STEP_FULL);
        current_step++;
        lcd.setCursor(current_step, 0);
        env_status = STATUS_HOLD;
        env_length = millis_per_step;
        button_note = 0;
    }
    if(current_step == 16) {
        current_step = 0;
        initMode(MODE_SEQ_EDIT);
    }
}

void loop_seq_edit() {

    if(printed_step_data != 0 && millis() - printed_step_data > 2000) {
        resetStepData();
    }

    if(status != STATUS_PLAY) {
        int k2 = analogRead(4);
        if(abs(k2-lastAnalogReadK2) > 3) {
            lastAnalogReadK2 = k2;
            modified_envmod = map(k2, 0, 1023, 1, 255);
            resetStepData();
            printKnobWithSign('E', map(modified_envmod, 1, 255, -50, 51));
        }
        int k3 = analogRead(5);
        if(abs(k3-lastAnalogReadK3) > 3) {
            lastAnalogReadK3 = k3;
            modified_decay = map(k3, 0, 1023, 1, 255);
            resetStepData();
            printKnobWithSign('D', map(modified_decay, 1, 255, -50, 51));
        }
        int k4 = analogRead(6);
        if(abs(k4-lastAnalogReadK4) > 3) {
            lastAnalogReadK4 = k4;
            modified_cutoff = map(k4, 0, 1023, 1, 255);
            resetStepData();
            printKnobWithSign('C', map(modified_cutoff, 1, 255, -50, 51));
        }
        int k5 = analogRead(7);
        if(abs(k5-lastAnalogReadK5) > 3) {
            lastAnalogReadK5 = k5;
            modified_glide = map(k5, 0, 1023, 0, 100);
            resetStepData();
            printKnob('G', modified_glide);
        }
    }

    loop_seq_common();
    switch(button_cmd) {
    case BUTTON_OCTUP:
        current_octave++;
        if(current_octave > 4) current_octave = 4;
        if(modified_pitch == 0) {
            resetStepData();
            lcd.setCursor(3, 1);
            lcd.print("--");
            lcd.print(current_octave);
        } else {
            int octave = modified_pitch / 12;
            int note = modified_pitch - (octave * 12);
            printNote(current_octave, note);
            modified_pitch = noteNumber(current_octave, note);
            resetStepData();
            printNoteNumber(modified_pitch);
        }
        lcd.setCursor(current_step, 0);
        button_cmd = 0;
        break;
    case BUTTON_OCTDN:
        current_octave--;
        if(current_octave < 1) current_octave = 1;
        if(modified_pitch == 0) {
            resetStepData();
            lcd.setCursor(3, 1);
            lcd.print("--");
            lcd.print(current_octave);
        } else {
            int octave = modified_pitch / 12;
            int note = modified_pitch - (octave * 12);
            resetStepData();
            printNote(current_octave, note);
            modified_pitch = noteNumber(current_octave, note);
            printNoteNumber(modified_pitch);
        }
        button_cmd = 0;
        break;
    case BUTTON_RIGHT:
        if(selected_step < 15) selected_step++;
        printNoteNumber(pattern[selected_step]);
        printStepData();
        modified_pitch = pattern[selected_step];
        modified_envmod = 0;
        modified_decay = 0;
        modified_cutoff = 0;
        modified_glide = 0;
        button_cmd = 0;
        break;
    case BUTTON_LEFT:
        if(selected_step > 0) selected_step--;
        printNoteNumber(pattern[selected_step]);
        printStepData();
        modified_pitch = pattern[selected_step];
        modified_envmod = 0;
        modified_decay = 0;
        modified_cutoff = 0;
        modified_glide = 0;
        button_cmd = 0;
        break;
    case BUTTON_ENTER:
        pattern[selected_step] = modified_pitch;
        if(modified_envmod != 0)
            pattern_envmod[selected_step] = modified_envmod;
        if(modified_decay != 0)
            pattern_decay[selected_step] = modified_decay;
        if(modified_cutoff != 0)
            pattern_cutoff[selected_step] = modified_cutoff;
        if(modified_glide != 0)
            pattern_glide[selected_step] = modified_glide;
        printStep(selected_step);
        if(selected_step < 15) selected_step++;
        printNoteNumber(pattern[selected_step]);
        printStepData();
        modified_pitch = pattern[selected_step];
        modified_envmod = 0;
        modified_decay = 0;
        modified_cutoff = 0;
        modified_glide = 0;
        button_cmd = 0;
        break;
    case BUTTON_REST:
        if(modified_pitch != 0) {
            modified_pitch = 0;
            resetStepData();
            lcd.setCursor(3, 1);
            lcd.print("--");
            lcd.print(current_octave);
        } else {
            if(selected_step > 0) {
                modified_pitch = 255;
                resetStepData();
                printNoteNumber(pattern[selected_step-1]);
                printStep(selected_step);
            }
        }
        button_cmd = 0;
        break;
    }

    if(button_note > 0) {
        modified_pitch = noteNumber(current_octave, button_note);
        if(status != STATUS_PLAY) {
            status = STATUS_PREVIEW_NOTE;
            writeToDAC(noteNumberToPitch(modified_pitch), current_cutoff);
            digitalWrite(3, HIGH);
        }
        button_note = 0;
    }

    if(status != STATUS_PLAY) {
        lcd.setCursor(selected_step, 0);
    }
}

void loop_menu() {
    if(button_cmd == BUTTON_MENU) {
        initMode(mode_last);
    }
}

void writePercent(int v, int x, int y) {
    lcd.setCursor(x, y);
    if(v < 10) {
        lcd.print(" ");
    }
    lcd.print(v);
}

byte readNoteButtons() {
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

byte readCmdButtons() {
    int a = analogRead(1);
    if(AROUND(a, 177)) {
        return BUTTON_ENTER;
    }
    if(AROUND(a, 157)) {
        return BUTTON_RIGHT;
    }
    if(AROUND(a, 136)) {
        return BUTTON_LEFT;
    }
    if(AROUND(a, 114)) {
        return BUTTON_REST;
    }
    if(AROUND(a, 92)) {
        return BUTTON_OCTDN;
    }
    if(AROUND(a, 67)) {
        return BUTTON_OCTUP;
    }
    if(AROUND(a, 42)) {
        return BUTTON_MENU;
    }
    if(AROUND(a, 15)) {
        return BUTTON_PLAY;
    }
    return 0;
}

byte readPattButtons() {
    int a = analogRead(2);
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
    lcd.setCursor(3, 1);
    bool s = false;
    switch(note) {
        case 1: lcd.print("C"); s = true; break;
        case 2: lcd.print("C#"); break;
        case 3: lcd.print("D"); s = true; break;
        case 4: lcd.print("D#"); break;
        case 5: lcd.print("E"); s = true; break;
        case 6: lcd.print("F"); s = true; break;
        case 7: lcd.print("F#"); break;
        case 8: lcd.print("G"); s = true; break;
        case 9: lcd.print("G#"); break;
        case 10: lcd.print("A"); s = true; break;
        case 11: lcd.print("Bb"); break;
        case 12: lcd.print("B"); s = true; break;
    }
    lcd.print(octave);
    if(s) lcd.print(" ");
}

void printNoteNumber(int note) {
    if(note == 0) {
        lcd.setCursor(3, 1);
        lcd.print("---");
    } else {
        int octave = note / 12;
        note = note - (octave * 12);
        printNote(octave, note);
    }
}

void printStep(int s) {
    lcd.setCursor(s, 0);
    if(pattern[s] == 0) {
        lcd.write(s % 4 == 0 ? CHAR_STEP_EMPTY_Q : CHAR_STEP_EMPTY);
    } else {
        if(pattern[s] == 255) {
            lcd.write(s % 4 == 0 ? CHAR_STEP_HOLD_Q : CHAR_STEP_HOLD);
        } else {
            if(pattern[s] > 72) {
                lcd.write(s % 4 == 0 ? CHAR_STEP_SLIDE_Q : CHAR_STEP_SLIDE);
            } else {
                lcd.write(s % 4 == 0 ? CHAR_STEP_FULL_Q : CHAR_STEP_FULL);
            }
        }
    }
}

void printPattern() {
    for(int i = 0; i < 16; i++) {
        printStep(i);
    }
    lcd.setCursor(current_step, 0);
}

void printKnob(char letter, int value) {
    lcd.setCursor(7, 1);
    lcd.print(letter);
    if(value < 100) {
        lcd.print(" ");
    }
    if(value < 10) {
        lcd.print(" ");
    }
    lcd.print(value);
}

void printKnobWithSign(char letter, int value) {
    lcd.setCursor(7, 1);
    lcd.print(letter);
    if(value == 0) {
        lcd.print("  ");
    } else {
        if(value > 0) {
            if(value < 10) {
                lcd.print(" ");
            }
            lcd.print("+");
        } else {
            if(value > -10) {
                lcd.print(" ");
            }
        }
    }
    lcd.print(value);
}

void printValueWithSign(int value) {
    if(value == 0) {
        lcd.print("  ");
    } else {
        if(value > 0) {
            if(value < 10) {
                lcd.print(" ");
            }
            lcd.print("+");
        } else {
            if(value > -10) {
                lcd.print(" ");
            }
        }
    }
    lcd.print(value);
}

void printStepData() {
    lcd.setCursor(7, 0);
    lcd.print("E");
    if(pattern_envmod[selected_step] == 0) {
        lcd.print("  0");
    } else {
        printValueWithSign(map(pattern_envmod[selected_step], 1, 255, -50, 50));
    }
    lcd.print(" D");
    if(pattern_decay[selected_step] == 0) {
        lcd.print("  0");
    } else {
        printValueWithSign(map(pattern_decay[selected_step], 1, 255, -50, 50));
    }
    lcd.setCursor(7, 1);
    lcd.print("C");
    if(pattern_cutoff[selected_step] == 0) {
        lcd.print("  0");
    } else {
        printValueWithSign(map(pattern_cutoff[selected_step], 1, 255, -50, 50));
    }
    lcd.print(" G");
    if(pattern_glide[selected_step] == 0) {
        lcd.print("  0");
    } else {
        printValueWithSign(map(pattern_glide[selected_step], 1, 255, -50, 50));
    }
    printed_step_data = millis();
}

void resetStepData() {
    printed_step_data = 0;
    printPattern();
    printTempo();
    lcd.setCursor(selected_step, 0);
}

void printTempo() {
    lcd.setCursor(12, 1);
    lcd.print("T");
    if(bpm < 100) {
        lcd.print(" ");
    }
    if(bpm < 10) {
        lcd.print(" ");
    }
    lcd.print(bpm);
}

int noteToPitch(int octave, int note) {
    int pitch = 768; // C2
    pitch += (octave - 2) * 648;
    pitch += (note - 1) * 54;
    return pitch;
}

int noteNumber(int octave, int note) {
#if DEBUG
    Serial.print("noteNumber(");
    Serial.print(octave);
    Serial.print(", ");
    Serial.print(note);
    Serial.print(")=");
    Serial.println(octave * 12 + note);
#endif
    return octave * 12 + note;
}

int noteNumberToPitch(int note) {
#if DEBUG
    Serial.print("noteNumberToPitch(");
    Serial.print(note);
    Serial.print(")=noteToPitch(");
#endif
    int octave = note / 12;
    note = note - (octave * 12);
#if DEBUG
    Serial.print(octave);
    Serial.print(", ");
    Serial.print(note);
    Serial.println(")");
#endif
    return noteToPitch(octave, note);
}

void nextStep() {
    current_step = current_step + 1;
    if(current_step == 16) current_step = 0;
    lcd.setCursor(current_step, 0);
    // if(current_step == selected_step && is_pitch_dirty) {
    //    writeToDAC(modified_pitch, current_cutoff);
    // } else {
        if(pattern[current_step] == 0) {
            digitalWrite(3, LOW);
            lcd.setCursor(3, 1);
            lcd.print("---");
        } else {
            initStep();
            writeToDAC(noteNumberToPitch(pattern[current_step]), current_cutoff);
            digitalWrite(3, HIGH);
            // printNoteNumber(pattern[current_step]);
        }
    // }
    // TODO glide & env
    lcd.setCursor(current_step, 0);
}

void createSeqChars() {
    byte char_step_empty[8] = {
      B00000,
      B00000,
      B01110,
      B01010,
      B01110,
      B00000,
      B00000,
    };
    byte char_step_empty_q[8] = {
      B00100,
      B00100,
      B01110,
      B01010,
      B01110,
      B00100,
      B00100,
    };
    byte char_step_full[8] = {
      B00000,
      B01110,
      B11111,
      B11111,
      B11111,
      B01110,
      B00000,
    };
    byte char_step_full_q[8] = {
      B00100,
      B01110,
      B11111,
      B11111,
      B11111,
      B01110,
      B00100,
    };
    byte char_step_hold[8] = {
      B00000,
      B01110,
      B10101,
      B10001,
      B10101,
      B01110,
      B00000,
    };
    byte char_step_hold_q[8] = {
      B00100,
      B01110,
      B10101,
      B10001,
      B10101,
      B01110,
      B00100,
    };
    byte char_step_slide[8] = {
      B00000,
      B01110,
      B10111,
      B11011,
      B10111,
      B01110,
      B00000,
    };
    byte char_step_slide_q[8] = {
      B00100,
      B01110,
      B10111,
      B11011,
      B10111,
      B01110,
      B00100,
    };
    lcd.createChar(CHAR_STEP_EMPTY, char_step_empty);
    lcd.createChar(CHAR_STEP_EMPTY_Q, char_step_empty_q);
    lcd.createChar(CHAR_STEP_FULL, char_step_full);
    lcd.createChar(CHAR_STEP_FULL_Q, char_step_full_q);
    lcd.createChar(CHAR_STEP_HOLD, char_step_hold);
    lcd.createChar(CHAR_STEP_HOLD_Q, char_step_hold_q);
    lcd.createChar(CHAR_STEP_SLIDE, char_step_slide);
    lcd.createChar(CHAR_STEP_SLIDE_Q, char_step_slide_q);
}
