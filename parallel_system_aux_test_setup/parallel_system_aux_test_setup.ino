/* Parallel System setup with Arduino uno, aux board 
 *  controls the pumps and floats and handles the experiement running
 *  Floats on D7 (PCINT7)
 *  Motor on D11 PWMed
 *  Skips D4 and D10 for option expansion via ethernet or SD card 
 *  SPI interface pins 13, 12, 11
 *  calculates flow volume added and sends data back to Serial port for recording
 *  also writes to a text file on an SD card
 *  uses a button on D6 to trigger system start
 *  LED on button lights up while system is active
 *  LED on pin 7, button on pin 6
 *  buzzer is on pin 8
 *  Records volume and interrupts at 100 ml and 500 ml mark
 *  to alert ofr sample withdrawal.
 *  
 */

#include <SPI.h> //for data logging
#include <SD.h>

//some defs about the setup
#define numFunnels 1 //for now

//pinout definitions , funnels start at 1, array at 1
const int motorPins[numFunnels+1] = {-1,3}; //the pinouts of the setup
const int floatPins[numFunnels+1] = {-1,7};

#define startPin 6
#define activeLED 9
#define bypassPin 5 //for bypassing the samples

#define buzzerPin 8 //buzzer output for sample done

#define chipSelect 4 //chip select pin for SD card

boolean start = false; //did we start yet?

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

//vars for determining which float is triggered 
volatile boolean floatStates[numFunnels+1]; //for storing float positions

//vars for pumps
volatile float influentVols[numFunnels+1]; //for storing volume going in
volatile uint32_t pumpTimes[numFunnels+1]; //for motor run times
volatile uint32_t checkVolume; //are we reading the volume of the influent
const float motorFlowRate = 1.87; //flow rate of motors in mL/s
const float checkInfluentVol = 1.0; //update time to check the influent vol of the motor
const int firstMeasureVol = 100; //sample collection at volume totals
const int secondMeasureVol = 600;

boolean sample1; 
boolean sample2; //did we take samples?

float currentTime; //for datalogging tracking teh current time
String dataString = ""; //for writing to the datalog file

void setup() {
  Serial.begin(9600); //for debugging
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.print("Initializing SD card..."); //setup SD card

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
  writeSD("Start of run"); //title
  writeSD(" "); //one line space
  
  pinMode(startPin, INPUT); //for button
  pinMode(activeLED, OUTPUT); //led
  pinMode(buzzerPin, OUTPUT);//buzzer
  
  for (int i = 1; i <= numFunnels; i++) { //set i/o pins
    pinMode(motorPins[i], OUTPUT);
    pinMode(floatPins[i], INPUT); //floats are on port K
    PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); //enable interrupts 
  }
  PCIFR  |= (1 << PCIF2);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE2); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  

}

void loop() {
  if (start) { //if button was pushed
    for (int i = 1; i <= numFunnels; i++) { 
      if (floatStates[i]) digitalWrite(motorPins[i], LOW); //if that float was activated turn off the motor to not overflow
      else digitalWrite(motorPins[i], HIGH); //otherwise this pump is on
      if (float(millis() - checkVolume)/1000 >= checkInfluentVol) { //if the time has elapsed, check the volume
        checkVolume = millis(); //store time of last check of volume
        getInfluentVols(i);
        if (influentVols[i] >= firstMeasureVol && !sample1) { //if the volume is at the 1st measure point
          start = false; //stop the run
          getSample(i); //retrieve sample on that funnel
          sample1 = true; //have now taken sample 1
        }else if (influentVols[i] >= secondMeasureVol && !sample2) { //if vol is at 2nd measure and not taken sample
          start = false; //stop the run
          getSample(i); //retrieve sample on that funnel
          sample2 = true; //have now taken sample 2
        }//otherwise not in challenge run so continue flowing liquid
      }
      if (digitalRead(bypassPin)) {
        for(int j = 0; j < 3; j++) { //flash thrice to acknowledge
          digitalWrite(activeLED, LOW);
          delay(500);
          digitalWrite(activeLED,HIGH);
          delay(500);
       }
        sample1 = true;
        sample2 = true; //bypass the sampling to allow automated dispensing
      }
    }
  }else{ //otherwise read the button
    if (digitalRead(startPin)) {
      for (int i = 1; i <= numFunnels; i++) { //scroll thru each pin and light up led
        pumpTimes[i] = millis(); //start time of pump interval for the next round
      }
      start = true;
      digitalWrite(activeLED, HIGH);
    }
  }
}

ISR (PCINT2_vect) { //when a button is pressed
 //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) { //reenables interrupts
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
      // want A8-A15 digitalPinToPCMSKbit function for convenience
      //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
    }
    for (int i = 0; i <= numFunnels; i++) { //need to start loop 1 entry before to allow proper update on 1st funnel
      if (!digitalRead(floatPins[i])) { //if that float is pushed
        
        Serial.print("Float: "); //some debug messages
        Serial.print(i);
        Serial.println(" is on");

        getInfluentVols(i); //append to influent toal since motor has just turned off
        floatStates[i] = true; //this float is on
        digitalWrite(motorPins[i], LOW); //turn off that motor
      }else{
        
        Serial.print("Float: "); //some debug messages
        Serial.print(i);
        Serial.println(" is off ");
     
        floatStates[i] = false; //that float is off, turn on the motor
        digitalWrite(motorPins[i], HIGH);
        pumpTimes[i] = millis(); //start pump interval since motor is back on
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
} 

void getInfluentVols(int index) { //tracks volume added for pump delinated by index
  if (!floatStates[index]) { //as long as motor is on i.e. float not active
    //current time minus the last pump active time times the average flow rate of the motor gives total volume
    influentVols[index] += ((float(millis() - pumpTimes[index])/1000)*motorFlowRate); //calculate change in volume
    currentTime += float(millis() - pumpTimes[index])/1000; //the detla t added gives current time
    pumpTimes[index] = millis(); //start time of pump interval for the next round
    //convert to the time to secs
  }
  Serial.print(currentTime);
  Serial.print(" Dispensed: "); //print to serial
  Serial.println(influentVols[index]);
  //add to the string so can add to our text file
  dataString += String(currentTime); //start with the time
  dataString += " ,"; //add a delineator
  dataString += String(influentVols[index]); //then influent vols
  writeSD(dataString);
}

void getSample(int index) { //allows sample collection of effluent for pump delineated by index
  digitalWrite(motorPins[index], LOW); //turn off that motor
  Serial.print("Taking Sample: ");
  writeSD(" ");
  writeSD("Took Sample"); //give some space
  writeSD(" "); 
  
  digitalWrite(buzzerPin, HIGH); //beep briefly to indicate done
  delay(2000);
  digitalWrite(buzzerPin, LOW);
  while (!digitalRead(startPin)) { //until the button is pressed
    digitalWrite(activeLED, LOW); //flash the led continuously
    delay(500);
    digitalWrite(activeLED, HIGH);
    delay(500);
  }
  start = true; //restart again after button press
}

void writeSD(String input) {
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(input);
    dataFile.close();
    dataString = ""; //clear the string for the next line
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}
