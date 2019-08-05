/* Parallel System setup with Arduino mega, aux board 
 *  controls the pumps and floats and handles the experiement running
 *  Floats on A8-A15 (D62-D69, PCINT16-23)
 *  Motors on D2, D3, D5, D6, D7, D8, D9, D11
 *  Skips D4 and D10 for expansion via ethernet or SD card 
 *  SPI interface pins 50, 51, 52, 53 
 *  calculates flow volume added and sends data back to Serial port for recording
 *  also writes to a text file on an SD card
 *  uses a button on D12 to trigger system start
 *  LED on button lights up while system is active
 *  LED on pin 13
 *  buzzer is on pin 22
 *  Calibration test program that runs each motor for 10 secs to determine its flow rate
 *  
 */

#include <SPI.h> //for data logging
#include <SD.h>

//some defs about the setup
#define numFunnels 8 //for now

//pinout definitions arrays start at 0 goto numFunnels, funnels are started at 1 so 1st entry is dummy
const int motorPins[numFunnels+1] = {-1, 11, 9, 8, 7, 6, 5, 3, 2}; //the pinouts of the setup
const int floatPins[numFunnels+1] = {-1, 62, 63, 64, 65, 66, 67, 68, 69};
//use slightly larger arrays to avoid going out of bounds

#define startPin 12
#define activeLED 13
#define bypassPin 23 //for bypassing the samples

#define buzzerPin 22 //buzzer output for sample done

#define chipSelect 4 //chip select pin for SD card

#define debounceTime 300  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

volatile boolean start = false; //did we start yet?
volatile int motorPointer = 1; //which motor are we on?
boolean hasRun = false; //did the current motor finish yet?
const uint32_t runTime = 20000; //run for 20 secs each
volatile uint32_t startTime;


void setup() {
  Serial.begin(9600); //for debugging

  //i/o
  pinMode(startPin, INPUT); //for button
  pinMode(activeLED, OUTPUT); //led
  pinMode(buzzerPin, OUTPUT);//buzzer

  for (int i = 1; i <= numFunnels; i++) { //set i/o pins
    pinMode(motorPins[i], OUTPUT);
    pinMode(floatPins[i], INPUT); //floats are on port K
  }
  PCMSK0 |=  (1 << digitalPinToPCMSKbit(startPin)); 
  PCIFR  |= (1 << PCIF0);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE0); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  

}

void loop() {
  if (start) { //if button was pushed at least once
    if (!hasRun) { //if motor hasnt run yet, run it
      digitalWrite(activeLED, HIGH);
      digitalWrite(motorPins[motorPointer], HIGH);
      Serial.print("Taking Sample "); 
      Serial.println(motorPointer);
      if (millis() - startTime > runTime) { //if reaches end of the loop
        digitalWrite(motorPins[motorPointer], LOW); //then stop
        beep(); //give an alert
        digitalWrite(activeLED, LOW);
        hasRun = true; //not its ran so stop
      }
    }
  } //if button wasnt pressed do nothing
}

ISR (PCINT0_vect) { //when a button is pressed
 //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    startTime = millis();
    if (!start) {
      start = true; //if we didnt start, begin on 1st press
    }else{ //if we did start move onto the next motor
      for (int j = 0; j <= numFunnels; j++) { //stops all motors
        digitalWrite(motorPins[j], LOW); //stops motors for safety
      }
      hasRun = false; //now onto next motor so allow it to run again
      motorPointer++; //go onto the next motor
      if (motorPointer > numFunnels) { //if at the end just stop the run
        start = false;
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
} 

void beep() { //beeps the buzzer and accomadates stopping the volume measurments
  for (int j = 0; j <= numFunnels; j++) {
    digitalWrite(motorPins[j], LOW); //stops motors for safety
  }
  digitalWrite(buzzerPin, HIGH); //beep briefly to indicate sample done
  delay(1000);
  digitalWrite(buzzerPin, LOW);
}
