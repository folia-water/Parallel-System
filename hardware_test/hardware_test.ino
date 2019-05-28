/* Sketch to test all the ahrdware setup, as described in schematic diagram. 
 *  TEsts LCd, LEDs and buttons on main arduino setup
 */


#define numFunnels 8 //pinout definitions
const int LEDpins[numFunnels+1] = {22, 23, 24, 25, 26, 27, 28, 29};
const int valvePins[6] = {44, 45, 46, 47 ,48 ,49};
const int buttonPins[numFunnels+1] = {62, 63, 64, 65, 66, 67, 68, 69};
volatile boolean buttonStates[numFunnels+1]; //for storing button pushes
volatile boolean buttonPressed = false; //is a button pushed?
#define buzzerPin 31 //for the beeper

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces
  
#include <LiquidCrystal.h> //use LCd

// initialize the library by associating any needed LCD interface pin
const int rs = 38, en = 39, d4 = 40, d5 = 41, d6 = 42, d7 = 43;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  lcd.begin(16,2);
  Serial.begin(9600); //for debugging
  for (int i = 0; i <= numFunnels; i++) { //set i/o pins
    pinMode(LEDpins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT);
  }
  for (int i = 0; i <= 6; i++) {
    pinMode(valvePins[i], OUTPUT);
  }
  for (int i = 0; i <= numFunnels; i++) {
    PCMSK2 |=  (1 << digitalPinToPCMSKbit(buttonPins[i])); 
    // want A8-A15 digitalPinToPCMSKbit function for convenience
    //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
  }
  PCIFR  |= (1 << PCIF2);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE2); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  

  lcd.print("Testing"); //beep quickly
  digitalWrite(buzzerPin, HIGH);
  delay(2000);
  digitalWrite(buzzerPin, LOW);
}

void loop() {
  for (int i = 0; i <= numFunnels-2; i++) { //scroll thru each pin and light up led
    if (!buttonPressed) {
      digitalWrite(LEDpins[i], HIGH);
      digitalWrite(valvePins[i], HIGH);
      delay(50);
      digitalWrite(LEDpins[i], LOW);
      digitalWrite(valvePins[i], LOW);
      delay(50);
    }else{ //if we did press a button 
      if (buttonStates[i]) { //if that button was pushed
        digitalWrite(LEDpins[i], HIGH); //light up that LED
      }
    }
  }
  for (int i = 7; i <= numFunnels; i++) { //second loop sincemore LEd pins than valves
    if (!buttonPressed) {
      digitalWrite(LEDpins[i], HIGH);
      delay(50);
      digitalWrite(LEDpins[i], LOW);
      delay(50);
    }else{ //if we did press a button 
      if (buttonStates[i]) { //if that button was pushed
        digitalWrite(LEDpins[i], HIGH); //light up that LED
      }
    }
  }
}

ISR (PCINT2_vect) { //when a button is pressed
  //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) {
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(buttonPins[i])); 
      // want A8-A15 digitalPinToPCMSKbit function for convenience
      //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
    }  
    buttonPressed = !buttonPressed; //if any button is pressed
    for (int i = 0; i <= numFunnels; i++) {
      if (digitalRead(buttonPins[i])) { //if that button is pushed
        buttonStates[i] = true;
        digitalWrite(LEDpins[i], HIGH); //light up that LED
      }else{
        buttonStates[i] = false;
        digitalWrite(LEDpins[i], LOW); //light up that LED
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
}  
