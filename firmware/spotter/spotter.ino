#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "NewEncoder.h"
#include <Bounce2.h>

#define DELAY_AFTER 500   // пауза после сварки
#define POT_MIN 20       // мин. время сварки, мс
#define POT_MAX 5000      // макс. время сварки, мс
//----------------PINS----------------
#define PIN_IN1 2 // encoder A pin
#define PIN_IN2 3 // encoder B pin
#define ENC_BTN_PIN 4 // encoder button
#define PIN_BTN 5   // main button
#define PIN_OUT 6   // transformator pin
//----------------CLASS OBJECTS----------------
Bounce mainButton = Bounce();
Bounce encButton = Bounce();
LiquidCrystal_I2C lcd(0x27, 16, 2);
//----------------WELDING DELAYS----------------
int delayFirstPulse; //first pulse delay
int delayPause; //pause delay
int delaySecondPulse; //second pulse delay
//----------------POINTER LCD----------------
int pointer = 0; // pointer on the lcd
bool pointerChanged = true;
//----------------ENCODER VARIABLES----------------
int16_t prevEncoderValue = 0; 
volatile bool newValue = false; //if the encoder value was chaged
volatile NewEncoder::EncoderState newState; //new encoder state

void ESP_ISR callBack(NewEncoder *encPtr, const volatile NewEncoder::EncoderState *state, void *uPtr); 
NewEncoder encoder(3, 2, -20, 20, 0, FULL_PULSE);

// made to economise some space on the 1602 lcd
byte p1_char[8] = {
  B00000,
  B00000,
  B11101,
  B10101,
  B11101,
  B10001,
  B10001,
  B00000
};

void setup() {
  // encoder setup
  encoder.begin();
  encoder.attachCallback(callBack);
  encButton.attach(ENC_BTN_PIN, INPUT_PULLUP); //enc button debounce
  encButton.interval(25); // enc button debounce delay
  // debounce of main button
  mainButton.attach(PIN_BTN, INPUT_PULLUP);
  mainButton.interval(25);
  pinMode(PIN_OUT, OUTPUT);
  // lcd init
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, p1_char);
}

void loop() {  
  encoderHandler();
  updateLCD();
  spotter();
}

void spotter() {
  mainButton.update();
  if (mainButton.fell()) {              // button press    
    lcd.setCursor(9, 1);
    lcd.print("Welding");
    digitalWrite(PIN_OUT, 1);               // turn on transformator
    if (!smartDelay(delayFirstPulse)) return;   // wait for the first pulse
    digitalWrite(PIN_OUT, 0);               // turn off transformator
    if (!smartDelay(delayPause)) return;   // wait the pause between pulses
    digitalWrite(PIN_OUT, 1);               // turn on transformator
    if (!smartDelay(delaySecondPulse)) return;     // wait for the second pulse
    digitalWrite(PIN_OUT, 0);               // turn off transformator
    while (mainButton.rose()) mainButton.update();          // await button realese
    lcd.setCursor(9, 1);
    lcd.print("        ");
  }
}

// smart delay, return false, if the button is realesd
bool smartDelay(int time) {
  uint32_t tmr = millis();
  while (millis() - tmr < time) {
    if (digitalRead(PIN_BTN)) {
      digitalWrite(PIN_OUT, 0);
      delay(DELAY_AFTER);
      return false;
    }
  }
  return true;
}

void encoderHandler(){
   int16_t currentValue;
   NewEncoder::EncoderClick currentClick;
  
   if (newValue){
     currentValue = newState.currentValue;
     newValue = false;
     int encSteps = currentValue - prevEncoderValue;
     if (encSteps != 0){
       int counterEnc = encSteps * 20;
       switch (pointer){
          case 0:
            delayFirstPulse += counterEnc;
            delayFirstPulse = constrain(delayFirstPulse, 0, 980);
            break;
          case 1:
            delaySecondPulse += counterEnc;
            delaySecondPulse = constrain(delaySecondPulse, 0, 980);
            break;
          case 2:
            delayPause += counterEnc;
            delayPause = constrain(delayPause, 0, 5000);
            break;
        }
        encoder.getAndSet(0);
     } 
   }
   // if button is pressed - move the pointer
   encButton.update();  
   if (encButton.fell()) {  
    if (pointer != 2) pointer++;
    else pointer = 0;  
    pointerChanged = true;
   }
}

void updateLCD(){
  lcd.setCursor(0, 0);
  lcd.print("p=" + String(delayFirstPulse) + "ms");
  if (delayFirstPulse < 100) lcd.print(" ");
  lcd.setCursor(8, 0);
  lcd.write(byte(0));
  lcd.setCursor(9, 0);
  lcd.print("=" + String(delaySecondPulse) + "ms");
  if (delaySecondPulse < 100) lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("t=" + String(delayPause) + "ms");
  if (delayPause < 1000) lcd.print(" ");
  if (pointerChanged){
    pointerChanged = !pointerChanged;
    lcd.clear();
    switch (pointer){
      case 0:
        lcd.setCursor(7, 0);
        lcd.print("<");
        break;
      case 1:
        lcd.setCursor(15, 0);
        lcd.print("<");
        break;
      case 2:
        lcd.setCursor(8, 1);
        lcd.print("<");
        break;
    }
  }
}

void ESP_ISR callBack(NewEncoder *encPtr, const volatile NewEncoder::EncoderState *state, void *uPtr) { // callback function of NewEncoder librarie
  (void) encPtr;
  (void) uPtr;
  memcpy((void *)&newState, (void *)state, sizeof(NewEncoder::EncoderState));
  newValue = true; 
}
