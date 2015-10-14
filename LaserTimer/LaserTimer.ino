/* Encoder Library - Basic Example
 * http://www.pjrc.com/teensy/td_libs_Encoder.html
 *
 * This example code is in the public domain.
 */

#include <Encoder.h>
#include <Wire.h>
#include <EEPROM.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <Adafruit_MCP4725.h>
#include <math.h>

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder encoder(3, 2);
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_MCP4725 dac;

// Constant Pins
const int buttonYel = 4;
const int buttonGrn = 5;
const int ttlPin = 6;
const int ledPins[] = {12, 13, 9, 10, 11, 8};

// States related to buttons
int state = 0;
int stimulating = 0;
int buttondn = 0;

// Values of each key variable of box
int freq = 10; // Frequency of stimulation (Hz)
int pulsedur = 10; // Pulse duration (ms)
float ontime = 1.0; // On time (sec, 1.2)
float offtime = 2.0; // Off time
int repetitions = 200; // repetitions (number by ten)
float volts = 1.90; // Amplitude (Volts, 1.22)

// Values for active variables during stimulation
long repn = 0;
unsigned long time = 0;
long oldPosition  = -999;
long int currenttime = 0;
int startupignoreencoder = 1;
int dacoutput = 0;

unsigned long int lastButtonPress = 0;
int currentButton = 0; // 1 for yellow, 2 for green
int buttonHold = 0;

struct StimData {
  int frequencyHz = 10;
  int pulseDurationMs = 10;
  float onDurationSec = 1.0;
  float offDurationSec = 2.0;
  int nRepetitions = 200;
  float voltageOutput = 1.90;
};

StimData pars;

void setup() {
  Serial.begin(9600);
  
  pinMode(buttonYel, INPUT_PULLUP);
  pinMode(buttonGrn, INPUT_PULLUP);
  pinMode(ttlPin, OUTPUT);
  
  for (int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
  
  digitalWrite(ledPins[0], HIGH);
  readEEPROM();
  matrix.begin(0x70);
  dac.begin(0x62);
  dac.setVoltage(0, false);
  changeState(0);
}

void readEEPROM() {
  EEPROM.get(0, pars);
}

void writeEEPROM() {
  EEPROM.put(0, pars);
}

void loop() {
  if (stimulating == 1) {
    if (!digitalRead(buttonGrn)) {
        if (millis() - lastButtonPress > 300) {
          resetLEDS();
          currentButton = 0;
          lastButtonPress = millis();
          buttonHold = 1;
          dac.setVoltage(0, false);
        }
    }
    else {
      calculatestim();
    }
  }
  
  // DC Stim
  else if (stimulating == 2) {
    if (digitalRead(buttonYel) && buttonHold == 1) {
      buttonHold = 0;
      lastButtonPress = millis();
      currentButton = 0;
      Serial.println("1");
    }
    else if ((!digitalRead(buttonGrn) || !digitalRead(buttonYel)) && buttonHold == 0 && currentButton == 0 && millis() - lastButtonPress > 200) {
      currentButton = 1;
      lastButtonPress = millis();
      Serial.println("2");
    }
    else if (digitalRead(buttonGrn) && digitalRead(buttonYel) && buttonHold == 0 && currentButton == 1 && millis() - lastButtonPress > 300) {
      dac.setVoltage(0, false);
      resetLEDS();
      currentButton = 0;
      lastButtonPress = millis();
      buttonHold = 1;
      Serial.println("3");
    }
  }
  
  else {
    long newPosition = encoder.read();
    if (newPosition != oldPosition) {
      if (startupignoreencoder == 0) {
        changeState(newPosition/4 - oldPosition/4);
        oldPosition = newPosition;
      }
      else {
        oldPosition = newPosition;
        startupignoreencoder = 0;
      }
    }
    
    if (!digitalRead(buttonGrn) && buttonHold == 1) {
      buttonHold = 1;
    }
    else if (digitalRead(buttonGrn) && digitalRead(buttonYel)&& buttonHold == 1) {
      buttonHold = 0;
      lastButtonPress = millis();
    }
    else if (!digitalRead(buttonGrn) && currentButton == 0 && millis() - lastButtonPress > 200) {
      currentButton = 2;
      lastButtonPress = millis();
    }
    else if (!digitalRead(buttonYel) && currentButton == 1 && millis() - lastButtonPress > 2000) {
      Serial.println("> 3000 ms");
      currentButton = 0;
      int tmpoutput = (int) ((pars.voltageOutput/5.0)*4095);
      dac.setVoltage(tmpoutput, false);
      stimulating = 2;
      
      matrix.writeDigitRaw(0,0); // specify position and code, 80 = 'r'
      matrix.writeDigitRaw(1,28); // specify position and code, 28 = 'u'
      matrix.writeDigitRaw(3,84); // specify position and code, 84 = 'n'
      matrix.writeDigitRaw(4,0); // specify position and code, 84 = ''
      matrix.writeDisplay();
      
      
      buttonHold = 1;
      digitalWrite(ledPins[state], LOW);
      
    }
    else if (!digitalRead(buttonYel) && currentButton == 0 && millis() - lastButtonPress > 200) {
      currentButton = 1;
      lastButtonPress = millis();
      Serial.println("Yello wbutton pressed");
    }
    else if (digitalRead(buttonGrn) && currentButton == 2 && millis() - lastButtonPress > 30) {
      currentButton = 0;
      lastButtonPress = millis();
      startstim();
    }
    else if (digitalRead(buttonYel) && currentButton == 1 && millis() - lastButtonPress > 30) {
      currentButton = 0;
      lastButtonPress = millis();
      incrementstate();
      Serial.println("> 30 ms");
    }
  }
}

// Frequency of stimulation (Hz)
// Pulse duration (ms)
// On time (sec, 1.2)
// Off time
// repetitions (number by ten)
// Amplitude (Volts, 1.22)

void incrementstate() {
  digitalWrite(ledPins[state], LOW);
  state = (state + 1)%6;
  changeState(0);
  digitalWrite(ledPins[state], HIGH);
}

void resetLEDS() {
  state = 0;
  digitalWrite(ledPins[state], HIGH);
  changeState(0);
  repn = 0;
  stimulating = 0;
}

void changeState(int change) {
  if (state == 0) {
    pars.frequencyHz += change;
    if (pars.frequencyHz < 1) {pars.frequencyHz = 1;}
    else if (pars.frequencyHz > 200) {pars.frequencyHz = 200;}
    matrix.print(pars.frequencyHz, DEC);
  }
  else if (state == 1) {
    if (pars.pulseDurationMs < 30) {
      pars.pulseDurationMs += change;
    }
    else {
      pars.pulseDurationMs += change*10;
      if (pars.pulseDurationMs < 29) {pars.pulseDurationMs = 29;}
    }
    if (pars.pulseDurationMs < 1) {pars.pulseDurationMs = 1;}
    else if (pars.pulseDurationMs > 1000) {pars.pulseDurationMs = 1000;}
    matrix.print(pars.pulseDurationMs, DEC);
  }
  else if (state == 2) {
    pars.onDurationSec += ((float) change)/10.0;
    if (pars.onDurationSec < 0.1) {pars.onDurationSec = 0.1;}
    else if (pars.onDurationSec > 99.00) {pars.onDurationSec = 99.00;}
    matrixFloat1(pars.onDurationSec);
  }
  else if (state == 3) {
    pars.offDurationSec += ((float) change)/10.0;
    if (pars.offDurationSec < 0.1) {pars.offDurationSec = 0.1;}
    else if (offtime > 99.00) {pars.offDurationSec = 99.00;}
    matrixFloat1(pars.offDurationSec);
  }
  else if (state == 4) {
    pars.nRepetitions += change*10;
    if (pars.nRepetitions < 10) {pars.nRepetitions = 10;}
    else if (pars.nRepetitions > 9990) {pars.nRepetitions = 9990;}
    matrix.print(pars.nRepetitions, DEC);
  }
  else if (state == 5) {
    pars.voltageOutput += ((float) change)/100.0;
    if (pars.voltageOutput < 0.01) {pars.voltageOutput = 0.01;}
    else if (pars.voltageOutput > 5.0) {pars.voltageOutput = 5.0;}
    matrixFloat2(pars.voltageOutput*0.9);
  }
  
  matrix.writeDisplay();
}

void matrixFloat2(float val) {
  int tens = (int) (val/10.0);
  int ones = (int) (val - tens*10.0);
  int tenths = (int) ((val - tens*10.0 - ones*1.0)*10.0);
  int hundredths = (int) round((val - tens*10.0 - ones*1.0 - tenths*0.1)*100.0);
  if (hundredths == 10) {
    hundredths = 0;
    tenths += 1;
    if (tenths == 10) {
      tenths = 0;
      ones += 1;
      if (ones == 10) {
        tens += 1;
        ones = 0;
      }
    }
  }
  
  if (tens > 0) {
    matrix.writeDigitNum(0, tens);
  }
  else {
    matrix.writeDigitRaw(0, 0);
  }
  
  //Serial.println(hundredths);
  
  matrix.writeDigitNum(1, ones, true);
  matrix.writeDigitNum(3, tenths);
  matrix.writeDigitNum(4, hundredths);
}

void matrixFloat1(float val) {
  int tens = (int) (val/10.0);
  int ones = (int) (val - tens*10.0);
  int tenths = (int) round((val - tens*10.0 - ones*1.0)*10.0);
  
  if (tenths == 10) {
    tenths = 0;
    ones += 1;
    if (ones == 10) {
      tens += 1;
      ones = 0;
    }
  }

  matrix.writeDigitRaw(0, 0);
  
  if (tens > 0) {
    matrix.writeDigitNum(1, tens);
  }
  else {
    matrix.writeDigitRaw(1, 0);
  }
  
  matrix.writeDigitNum(3, ones, true);
  matrix.writeDigitNum(4, tenths);
}

void calculatestim() {
  int tmpoutput = 0;
  currenttime = micros() - time;
  if (currenttime <= pars.onDurationSec*1000000) {
    float window = 1000000.0/pars.frequencyHz;
    if (fmod((float) currenttime, window) < pars.pulseDurationMs*1000.0) {
      tmpoutput = (int) ((pars.voltageOutput/5.0)*4095);
    }
  }
  else if (currenttime > pars.onDurationSec*1000000 + pars.offDurationSec*1000000) {
    repn += 1;
    
    if (repn > pars.nRepetitions) {
      resetLEDS();
      return;
    }
    
    time = micros();
    matrix.print(repn, DEC);
    matrix.writeDisplay();
  }
  
  if (tmpoutput != dacoutput) {
    dacoutput = tmpoutput;
    dac.setVoltage(dacoutput, false);
    if (dacoutput > 0) {
      digitalWrite(ttlPin, HIGH);
    }
    else {
      digitalWrite(ttlPin, LOW);
    }
  }
}


void startstim() {
  matrix.writeDigitRaw(0,80); // specify position and code, 80 = 'r'
  matrix.writeDigitRaw(1,28); // specify position and code, 28 = 'u'
  matrix.writeDigitRaw(3,84); // specify position and code, 84 = 'n'
  matrix.writeDigitRaw(4,0); // specify position and code, 84 = ''
  matrix.writeDisplay();
  
  digitalWrite(ledPins[state], LOW);
  writeEEPROM();
  
  repn = 0;
  time = micros();
  stimulating = 1;
}
