/**
 * Oscilloscope based on Arduino and Nokia LCD 5110/3310.
 * @url http://srukami.inf.ua/pultoscop_v25110.html
 * @author srukami <srukamiua@gmail.com>
 * @website http://srukami.inf.ua
 *
 * The MIT License
 *
 * Copyright (c) 2016 Srukami <srukamiua@gmail.com> http://srukami.inf.ua/
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.The MIT License
 */
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <FreqCount.h>
#include <PWM.h>
#include <CyberLib.h>

/**
 * Settings block
 */
#define DEBUG 0
#define VCC 5.0
#define ARDUINO_FREQUENCY 16

#define DISPLAY_CONTRAST 52
#define DISPLAY_SCLK_PIN 7
#define DISPLAY_DIN_PIN 6
#define DISPLAY_DC_PIN 4
#define DISPLAY_CS_PIN 3
#define DISPLAY_RST_PIN 2

#define STATE_MENU 0
#define STATE_OSCILLOSCOPE 1
#define STATE_GENERATOR 2
#define STATE_DDS_GENERATOR 3
#define STATE_TERMINAL 4

#define BTN_MINUS_PIN 13
#define BTN_OK_PIN 12
#define BTN_PLUS_PIN 11
#define BTN_PRESS_DELAY 45

#define OSCILL_STATE_MEASURE 0
#define OSCILL_STATE_SCAN 1
#define OSCILL_STATE_PAUSE 2
#define OSCILL_STATE_REF_VOLTAGE 3
#define OSCILL_STATE_SYNCH 4

/**
 * Variables block
 */
 
// states
int state = STATE_MENU;
int menuState = STATE_OSCILLOSCOPE;

// buttons
bool minusBtnPressed = false;
bool okBtnPressed = false;
bool plusBtnPressed = false;
unsigned long btnPressTime = 0;

// oscilloscope
int oscillState = OSCILL_STATE_MEASURE;
bool oscillPaused = false;
bool oscillRefVoltage = false;
int oscillVMax = 0;
byte oscillScan = 0;
byte oscillSynU = 30;
byte oscillSynMass = 0;
byte oscillMass[701];
int oscillScroll = 0;
unsigned long oscillFreqCount = 0;
long oscillFreqCountX = 0;

// display
Adafruit_PCD8544 display = Adafruit_PCD8544(
  DISPLAY_SCLK_PIN, 
  DISPLAY_DIN_PIN, 
  DISPLAY_DC_PIN, 
  DISPLAY_CS_PIN, 
  DISPLAY_RST_PIN
);

/**
 * Menu
 */
void menu() {
  // process buttons
  if (okBtnPressed) {
    state = menuState;
    return;
  }
  if (plusBtnPressed) {
    menuState++;
    if (menuState > STATE_TERMINAL) {
      menuState = STATE_OSCILLOSCOPE;
    }
  }
  if (minusBtnPressed) {
    menuState--;
    if (menuState < STATE_OSCILLOSCOPE) {
      menuState = STATE_TERMINAL;
    }
  }
  // menu drawing
  display.setCursor(0,0);
  if (menuState == STATE_OSCILLOSCOPE) {
    display.setTextColor(WHITE, BLACK);
  } else {
    display.setTextColor(BLACK);
  }
  display.println("Oscilloscope");
  display.setCursor(0,10);
  if (menuState == STATE_GENERATOR) {
    display.setTextColor(WHITE, BLACK);
  } else {
    display.setTextColor(BLACK);
  }
  display.println("Generator");
  display.setCursor(0,20);
  if (menuState == STATE_DDS_GENERATOR) {
    display.setTextColor(WHITE, BLACK);
  } else {
    display.setTextColor(BLACK);
  }
  display.println("DDS Generator");
  display.setCursor(0,30);
  if (menuState == STATE_TERMINAL) {
    display.setTextColor(WHITE, BLACK);
  } else {
    display.setTextColor(BLACK);
  }
  display.println("Terminal");
  display.setCursor(0,40);
}

/**
 * Oscilloscope
 */
void oscilloscope() {
  byte x = 0;
  if (! oscillRefVoltage) { // Internal reference voltage 1,1V
    ADMUX = 0b11100011;
  } else { // External reference voltage
    ADMUX = 0b01100011;
  }
  if (! oscillPaused) {
    if (oscillScan >= 6) { ADCSRA = 0b11100010; } // divider 4
    if (oscillScan == 5) { ADCSRA = 0b11100011; } // divider 8
    if (oscillScan == 4) { ADCSRA = 0b11100100; } // divider 16
    if (oscillScan == 3) { ADCSRA = 0b11100101; } // divider 32
    if (oscillScan == 2) { ADCSRA = 0b11100110; } // divider 64
    if (oscillScan < 2) { ADCSRA = 0b11100111; } // divider 128
    if (oscillScan == 0) {
      for(int i=0; i<700; i++){ 
        while ((ADCSRA & 0x10) == 0);
        ADCSRA |= 0x10;
        // @TODO Rewrite this part to use millis() (example in updateButtons() function)
        // Because now this delay is affects the UI drawing speed
        // delayMicroseconds(500);
        oscillMass[i] = ADCH;
      }
    }
    if (oscillScan > 0) {
      for (int i=0;i < 700;i++){ 
        while ((ADCSRA & 0x10)==0);
        ADCSRA |= 0x10;
        oscillMass[i] = ADCH;
      }
    }
  }
  // find synchronization point
  bool flagSINHRO = 0;
  bool flagSINHRnull = 0;
  for (int y=1; y<255; y++) {
    if (flagSINHRO == 0 && oscillMass[y] < oscillSynU) {
      flagSINHRnull=1;
    }
    if (flagSINHRO == 0 && flagSINHRnull == 1 && oscillMass[y] > oscillSynU) {
      flagSINHRO=1;
      oscillSynMass=y;
    }
  }
  // signal max
  oscillVMax=0;
  for (int y=1; y<255; y++) {
    if (oscillVMax<oscillMass[y]) {
      oscillVMax=oscillMass[y];
    }
  }
  // chart drawing
  if (! oscillPaused) {
    display.fillCircle(80,47-oscillSynU/7, 2, BLACK); // synchronization level drawing
    x=3;
    for (int y=oscillSynMass; y<oscillSynMass+80; y++) {
      if (oscillScan < 7) { x++; }
      if (oscillScan == 7) { x=x+2; }
      if (oscillScan == 8) { x=x+3; }
      display.drawLine(x, 47-oscillMass[y]/7, x+1, 47-oscillMass[y+1]/7, BLACK);
      display.drawLine(x+1, 47-oscillMass[y]/7+1, x+2, 47-oscillMass[y+1]/7+1, BLACK);
    }
    oscillSynMass = 0;
  } else {
    display.drawLine(oscillScroll/8,8,oscillScroll/8+6,8, BLACK); // scroll scale
    display.drawLine(oscillScroll/8,9,oscillScroll/8+6,9, BLACK); // scroll scale
    x = 3;
    for (int y=oscillScroll; y<oscillScroll+80; y++) {
      if (oscillScan < 7) { x++; }
      if (oscillScan == 7) { x=x+2; }
      if (oscillScan == 8) { x=x+3; }
      display.drawLine(x, 47-oscillMass[y]/7, x+1, 47-oscillMass[y+1]/7, BLACK);
      display.drawLine(x+1, 47-oscillMass[y]/7+1, x+2, 47-oscillMass[y+1]/7+1, BLACK);
    }
  }
  // vertical marking
  for (byte i=47; i>5; i=i-7) {
    display.drawPixel(0, i, BLACK);
    display.drawPixel(1, i, BLACK);
    display.drawPixel(2, i, BLACK);
  }
  // grid
  for (byte i=47; i>5; i=i-3) {
    display.drawPixel(21, i, BLACK);
    display.drawPixel(42, i, BLACK);
    display.drawPixel(63, i, BLACK);
  }
  for (byte i=3; i<84; i=i+3) {
    display.drawPixel(i, 33, BLACK);
    display.drawPixel(i, 19, BLACK);
  }
  // menu drawing
  if (oscillState == OSCILL_STATE_MEASURE) {
    display.setCursor(0,0);
    display.setTextColor(WHITE,BLACK);
    if (! oscillRefVoltage) {
      display.print("1.1");
    } else {
      display.print(VCC,1);
    }
    display.setTextColor(BLACK);
    display.print(" ");
    display.print(oscillScan);
    display.print(" P");
    if (minusBtnPressed || plusBtnPressed) {
      oscillRefVoltage = !oscillRefVoltage;
    }
  }
  if (oscillState == OSCILL_STATE_SCAN) {
    display.setCursor(0, 0);
    display.setTextColor(BLACK);
    if (oscillRefVoltage == 0) {
      display.print("1.1");
    }
    if (oscillRefVoltage == 1) {
      display.print(VCC, 1);
    }
    display.setTextColor(WHITE, BLACK); 
    display.print(" ");
    display.print(oscillScan);
    display.setTextColor( BLACK); 
    display.print(" P");
    if (minusBtnPressed) {
      oscillScan--;
      if (oscillScan == 255) {
        oscillScan = 0;
      }
    }
    if (plusBtnPressed) {
      oscillScan++;
      if (oscillScan == 9) {
        oscillScan = 8;
      }
    }
  }
  if (oscillState == OSCILL_STATE_PAUSE) {
    display.setCursor(0, 0);
    display.setTextColor(BLACK);
    if (oscillRefVoltage == 0) {
      display.print("1.1");
    }
    if (oscillRefVoltage == 1) {
      display.print(VCC, 1);
    }
    display.print(" ");
    display.print(oscillScan);
    display.setTextColor(WHITE, BLACK); 
    display.print(" P");
    oscillPaused = true;
    if (minusBtnPressed) {
      oscillScroll = oscillScroll-10;
      if (oscillScroll < 0) {
        oscillScroll = 0;
      }
    }
    if (plusBtnPressed) {
      oscillScroll = oscillScroll + 10;
      if (oscillScroll > 620) {
        oscillScroll = 620;
      }
    }
  }
  if (oscillState == OSCILL_STATE_SYNCH) {
    oscillScroll=0;
    oscillPaused = false;
    display.setCursor(0, 0);
    display.setTextColor( BLACK);
    if (oscillRefVoltage == 0) {
      display.print("1.1");
    }
    if (oscillRefVoltage == 1) {
      display.print(VCC, 1);
    }
    display.print(" ");
    display.print(oscillScan);
    display.setTextColor(BLACK);
    display.print(" P");
    if (minusBtnPressed) {
      oscillSynU = oscillSynU-20;
      if (oscillSynU < 20) {
        oscillSynU = 20;
      }
    }
    if (plusBtnPressed) {
      oscillSynU = oscillSynU + 20;
      if (oscillSynU > 230) {
        oscillSynU = 230;
      }
    }
    display.fillCircle(80,47-oscillSynU/7, 5, BLACK);
    display.fillCircle(80,47-oscillSynU/7, 2, WHITE);
  }
  // walking through menu items
  if (okBtnPressed) {
    oscillState++;
    if (oscillState > OSCILL_STATE_SYNCH) {
      oscillState = OSCILL_STATE_MEASURE;
      oscillPaused = false;
    }
  }
  // get freequency when conter is ready
  if (FreqCount.available()) {
    oscillFreqCount = FreqCount.read();
  }
  // signal frequency
  byte Frec1 = 0;
  long Frec = 0;
  bool flagFrec1 = 0;
  bool flagFrec2 = 0;
  bool flagFrec3 = 0;
  for (int y=1; y<255; y++) {
    if (flagFrec1 == 0 && oscillMass[y] < oscillSynU) {
      flagFrec2 = 1;
    }
    if (flagFrec1 == 0 && flagFrec2 == 1 && oscillMass[y] > oscillSynU) {
      flagFrec1 = 1;
      Frec1 = y;
    }
    if (flagFrec1 == 1 && oscillMass[y] < oscillSynU) {
      flagFrec3 = 1;
    }
    if (flagFrec3 == 1 && oscillMass[y]>oscillSynU){
      if (oscillScan >= 6) { Frec = 1000000/((y-Frec1-1)*3.27); } // divider 4
      if (oscillScan == 5) { Frec = 1000000/((y-Frec1)*3.27)/2; } // divider 8
      if (oscillScan == 4) { Frec = 1000000/((y-Frec1)*3.27)/4; } // divider 16
      if (oscillScan == 3) { Frec = 1000000/((y-Frec1)*3.27)/8; } // divider 32
      if (oscillScan == 2) { Frec = 1000000/((y-Frec1)*3.27)/16; } // divider 64
      if (oscillScan == 2) { Frec = 1000000/((y-Frec1)*3.27)/32; } // divider 128
      if (oscillScan == 1) { Frec = 1000000/((y-Frec1)*3.27)/32; } // divider 128
      if (oscillScan == 0) { Frec = 1000000/((y-Frec1)*500); } // divider 128
      flagFrec1 = 0;
      flagFrec3 = 0;
    }
  }
  display.setTextColor(BLACK);
  if (oscillRefVoltage == 1) {
    if ((oscillVMax*VCC/255) > 2.5) {
      oscillFreqCountX = oscillFreqCount * (ARDUINO_FREQUENCY / 16.0);
    }
    if ((oscillVMax*VCC/255) < 2.5) {
      oscillFreqCountX = Frec * (ARDUINO_FREQUENCY / 16.0);
    }
  }
  if (oscillRefVoltage == 0) {
    oscillFreqCountX = Frec * (ARDUINO_FREQUENCY / 16.0);
  }
  if (oscillFreqCountX < 1000) {
    display.print(" ");
    display.print(oscillFreqCountX);
    display.print("Hz");
  }
  if (oscillFreqCountX > 1000) {
    float oscillFreqCountXK = oscillFreqCountX / 1000.0;
    display.print(oscillFreqCountXK, 1);
    display.print("KHz");
  }
  if (oscillRefVoltage == 1) {
    display.setCursor(0, 40);
    display.setTextColor(BLACK);
    display.print(oscillVMax * VCC / 255, 1);
  }
  if (oscillRefVoltage == 0) {
    display.setCursor(0, 40);
    display.setTextColor(BLACK);
    display.print(oscillVMax * 1.1 / 255, 1);
  }
  display.print("V");
}

/**
 * Generator
 */
void generator() {
  display.println("Generator");
}

/**
 * DDS Generator
 */
void ddsGenerator() {
  display.println("DDS Generator");
}

/**
 * Terminal
 */
void terminal() {
  display.println("Terminal");
}

/**
 * Update buttons states
 */
void updateButtons() {
  minusBtnPressed = false;
  okBtnPressed = false;
  plusBtnPressed = false;
  bool _minusBtnPressed = false;
  bool _okBtnPressed = false;
  bool _plusBtnPressed = false;
  bool pressed = false;
  if (digitalRead(BTN_MINUS_PIN) == HIGH) {
    pressed = true;
    _minusBtnPressed = true;
  }
  if (digitalRead(BTN_OK_PIN) == HIGH) {
    pressed = true;
    _okBtnPressed = true;
  }
  if (digitalRead(BTN_PLUS_PIN) == HIGH) {
    pressed = true;
    _plusBtnPressed = true;
  }
  if (pressed) {
    if (btnPressTime == 0) {
      btnPressTime = millis();
    }
    if (btnPressTime > 0 && (millis() - btnPressTime) > BTN_PRESS_DELAY) {
      minusBtnPressed = _minusBtnPressed;
      okBtnPressed = _okBtnPressed;
      plusBtnPressed = _plusBtnPressed;
      btnPressTime = 0;
    }
  } else {
    btnPressTime = 0;
  }
}

/**
 * Setup
 */
void setup() {
  // display
  display.begin();
  display.setContrast(DISPLAY_CONTRAST);
}

/**
 * Loop
 */
void loop() {
  // display set defaults
  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  // receive buttons states
  updateButtons();
  // process buttons
  if (minusBtnPressed && plusBtnPressed) {
    state = STATE_MENU;
  }
  // process selected state
  switch (state) {
    case STATE_MENU:
      menu();
      break;
    case STATE_OSCILLOSCOPE:
      oscilloscope();
      break;
    case STATE_GENERATOR:
      generator();
      break;
    case STATE_DDS_GENERATOR:
      ddsGenerator();
      break;
    case STATE_TERMINAL:
      terminal();
      break;
  }
  // render
  display.display();
}

