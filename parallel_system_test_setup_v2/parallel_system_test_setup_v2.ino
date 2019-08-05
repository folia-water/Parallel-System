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
 *  Records volume when all floats have triggered the first time
 *  tHis is the 100 ml effluent sample point
 *  beep when reaches 600 mL added, as this is the second sample point
 *  to alert for sample withdrawal.
 *  
 */

#include <SPI.h> //for data logging
#include <SD.h>

//some defs about the setup
#define numFunnels 4 //how many funnels are we testing
#define numFunnelsTotal 8 //the setup max amount

//pinout definitions arrays start at 0 goto numFunnels, funnels are started at 1 so 1st entry is dummy
const int motorPins[numFunnelsTotal+1] = {-1, 11, 9, 8, 7, 6, 5, 3, 2}; //the pinouts of the setup
const int floatPins[numFunnelsTotal+1] = {-1, 62, 63, 64, 65, 66, 67, 68, 69};
//use slightly larger arrays to avoid going out of bounds

#define startPin 12
#define activeLED 13
#define bypassPin 23 //for bypassing the samples

#define buzzerPin 22 //buzzer output for sample done

#define chipSelect 4 //chip select pin for SD card

boolean start = false; //did we start yet?

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

//vars for determining which float is triggered 
volatile boolean floatStates[numFunnelsTotal+1]; //for storing float positions

//vars for pumps
volatile float influentVols[numFunnelsTotal+1]; //for storing volume going in
volatile uint32_t pumpTimes[numFunnelsTotal+1]; //for motor run times
volatile uint32_t checkVolume; //are we reading the volume of the influent

//each motor has its own flow rate, starts at motor 1
const float motorFlowRate[numFunnelsTotal+1] = {0, 
1.55,  //flow rates calculated for each motor based on 20 sec tests
1.49, 
1.79,
1.91, //updated 1st 4 flow rates based on runs done
1.46,
1.73,
1.56,
1.44}; //flow rate of motors in mL/s

const float checkInfluentVol = 1.0; //update time to check the influent vol of the motor
const int sampleVol = 550; //sample collection at volume total of 600 ml
const int purgeDelay = 15000; //delay of 15 secs to purge lines of air and such
int maxInfluentVol = 1000; //for the 1 L of sample
const int bypassVolume = 4000; //max volume allowed for bypassed runs

boolean firstSample = false; //did we take the first sample?
boolean sample[numFunnelsTotal+1]; //did we take samples?
boolean flasher; //for flashing the LED, will be replaced by timers later
volatile int samplePointer; //have all the samples been collected
volatile boolean hasTaken[numFunnelsTotal+1]; //have we taken all the samples yet?
volatile boolean takenFirstSample; //have we taken the 1st sample

float currentTime; //for datalogging tracking the current time
String dataString = ""; //for writing to the datalog file

void setup() {
  Serial.begin(9600); //for debugging
  Serial.print("Initializing SD card..."); //setup SD card

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
  writeSD("Start of run 1"); //title
  writeSD(" "); //one line space
  writeSD("Funnel:,");
  writeSD(" ");

  //i/o
  pinMode(startPin, INPUT); //for button
  pinMode(activeLED, OUTPUT); //led
  pinMode(buzzerPin, OUTPUT);//buzzer

  dataString += "           ";
  for (int i = 1; i <= numFunnels; i++) { //set i/o pins
    pinMode(motorPins[i], OUTPUT);
    pinMode(floatPins[i], INPUT); //floats are on port K
    dataString += String(i); //create a header of numbers in the data file
    dataString += ",        ";
  }
  Serial.println(dataString);
  writeSD(dataString); //write to SD
  writeSD(" "); //add a line space
  
  PCIFR  |= (1 << PCIF2);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE2); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  
  PCIFR  |= (1 << PCIF0);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE0); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  


}

void loop() {
  if (start) { //if button was pushed
    for (int i = 0; i <= numFunnels; i++) { 
      if (floatStates[i] || sample[i]) digitalWrite(motorPins[i], LOW); //if that float was activated turn off the motor to not overflow
      //or if that funnel is at a sample point
      else digitalWrite(motorPins[i], HIGH); //otherwise this pump is on
      //only if the float isnt, and they havent passed sample points
      if (!firstSample && samplePointer >= numFunnels) { //if all the floats are active time for first sample
        for (int j = 1; j <= numFunnels; j++) {
          digitalWrite(motorPins[i], LOW); //stop all motors
          sample[j] = true; //are taking a sample
          PCMSK2 |=  (0 << digitalPinToPCMSKbit(floatPins[j])); //disable floats temporarily
        }
        PCMSK0 |=  (1 << digitalPinToPCMSKbit(startPin)); //for the start button enable sample registered
        getSample(0); //beep and record stuff
      }
      
      if (digitalRead(bypassPin)) {
        beep();
        for(int j = 0; j < 3; j++) { //flash thrice to acknowledge
          digitalWrite(activeLED, LOW);
          delay(500);
          digitalWrite(activeLED,HIGH);
          delay(500);
        }
        for (int j = 0; j <= numFunnels; j++) {
          //sample[j] = true; //bypass the sampling to allow automated dispensing
          hasTaken[j] = true;
          takenFirstSample = true;
          firstSample = true;
        }
        maxInfluentVol = bypassVolume; //if pressed for pure water run now
      } //end of bypass pin press
    }
    if (float(millis() - checkVolume)/1000 >= checkInfluentVol) { //if the time has elapsed, check the volume
      checkVolume = millis(); //store time of last check of volume
      currentTime += float(millis() - (currentTime*1000))/1000; //the delta t added gives current time
      //add to the string so can add to our text file
      dataString = ""; //clear data string to start a new line
      dataString += String(currentTime); //on the data file start with the time on left most column
      dataString += ",    ";
      for (int i = 1; i <= numFunnels; i++) {
        getInfluentVols(i); //print the data of volume inputted
        //read all floats if all active for the first time, then activate sample
        if (influentVols[i] >= sampleVol && !sample[i] && takenFirstSample && !hasTaken[i]) { //if the volume is at the measure point
          //and we havent already taken a sample
          digitalWrite(motorPins[i], LOW); //stop that motor
          sample[i] = true; //have now taking sample on that funnel
          PCMSK0 |=  (1 << digitalPinToPCMSKbit(startPin)); //for the start button enable sample registered
          getSample(i);
        } //if not sampling continue running as normal

        if (influentVols[i] >= maxInfluentVol && takenFirstSample && hasTaken[i]) { //if reached the end of the run
          sample[i] = true; //stop the motor
          digitalWrite(motorPins[i], LOW); 
          //just wait until others have finished
        }
        if (sample[i]) { //if any sample is ready, flash the LED
          flasher = !flasher;
          if (flasher) digitalWrite(activeLED, HIGH);
          else digitalWrite(activeLED, LOW);
        }
      }
      Serial.println(dataString); //print whats being written to Serial
      writeSD(dataString); //only after all 8 funnels have been written to the string
    }
  }else{ //otherwise read the button
    if (digitalRead(startPin)) { //when pressed start
      digitalWrite(activeLED, HIGH); //turn on active led
      for (int i = 1; i <= numFunnels; i++) { //scroll thru eachfunnel
        pumpTimes[i] = millis(); //start time of pump interval for the next round
      }
      start = true;
      for (int i = 0; i <= numFunnels; i++) { //enables interrupts
        PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
        // want A8-A15 digitalPinToPCMSKbit function for convenience
        //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
      }
    }
  }
}

ISR (PCINT2_vect) { //when a button is pressed
 //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) { //need to start loop 1 entry before to allow proper update on 1st funnel
      if (!digitalRead(floatPins[i])) { //if that float is pushed
        
        Serial.print("Float: "); //some debug messages
        Serial.print(i);
        Serial.println(" is on");
        
        if (i >= 1)  getInfluentVols(i); //append to influent total since motor has just turned off
        //only get influents on the actual motors, ot the 0th dummy one
        floatStates[i] = true; //this float is on
        digitalWrite(motorPins[i], LOW); //turn off that motor
        if (!firstSample) samplePointer ++; //count samples based on if floats are active for the first sample
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
    for (int i = 0; i <= numFunnels; i++) { //reenables interrupts after end of isr
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
      // want A8-A15 digitalPinToPCMSKbit function for convenience
      //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
    }
  }
} 

ISR (PCINT0_vect) { //when a button is pressed
 //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) { //need to start loop 1 entry before to allow proper update on 1st funnel
      pumpTimes[i] = millis(); //update time motors were off for
      if (sample[i] && takenFirstSample) { //if that sample was reached, have cleared it now, and have taken the sample
        hasTaken[i] = true; //have now taken samples from these funnels
        sample[i] = false; //only reset that particular sample
      }else if (sample[i]) { //if it was after the 1st sample
        takenFirstSample = true;
        for (int j = 1; j <= numFunnels; j++) { //no longer taking samples
          sample[j] = false; //are no longer taking any samples
          pumpTimes[j] = millis(); //update time motors were off for
          PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[j])); //reenable floats
        }
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
} 


void getInfluentVols(int index) { //tracks volume added for pump delinated by index
  dataString += String(influentVols[index]); //then influent vol;
  dataString += ",     "; // space between funnels
  if (!floatStates[index] && !sample[index]) { //as long as motor is on i.e. float not active
    //or the motor isnt stopped awaitng other funnels to catch up and sample
    //current time minus the last pump active time times the average flow rate of the motor gives total volume
    influentVols[index] += ((float(millis() - pumpTimes[index])/1000)*motorFlowRate[index]); //calculate change in volume
    pumpTimes[index] = millis(); //start time of pump interval for the next round
    //convert to the time to secs
  }
}

void getSample(int index) { //allows sample collection of effluent for pump delineated by index
  if (firstSample && !hasTaken[index]) { //if weve taken the first sample already
    //and previously never taken before
    dataString = ""; //write message for sample collection
    dataString += "Taking Sample From funnel: ";
    dataString += String(index);
    Serial.println(" ");
    Serial.println(dataString); 
    Serial.println(" ");
    writeSD(" ");
    writeSD(dataString); //give some space
    writeSD(" ");
    beep();
  }else{ //if havent taken 1st samnple, we are on the 1st
    dataString = ""; //write message for sample collection
    dataString += "Taking Samples From all funnels "; //first sample is from all funnels
    Serial.println(" ");
    Serial.println(dataString); 
    Serial.println(" ");
    writeSD(" ");
    writeSD(dataString); //give some space
    writeSD(" ");
    beep();
    firstSample = true; //now taken the 1st sample
  }
}

void beep() { //beeps the buzzer and accomadates stopping the volume measurments
  for (int j = 0; j <= numFunnels; j++) {
    digitalWrite(motorPins[j], LOW); //stop all motors briefly
    getInfluentVols(j); //update totals
  }
  digitalWrite(buzzerPin, HIGH); //beep briefly to indicate sample
  delay(1000); //stopped motors while beeping to avoid miscalcualtion of volumes
  digitalWrite(buzzerPin, LOW);
  for (int j = 0; j <= numFunnels; j++) {
    digitalWrite(motorPins[j], HIGH); //start motors again
    pumpTimes[j] = millis(); //update totals
  } //stops all motors to allow accurate measurement during duration of beep
}

void writeSD(String input) { //function takes a string and writes it to one line of a text file on an SD card
  File dataFile = SD.open("datalog.txt", FILE_WRITE); //open and create file object
  
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(input);//prinbt with new lines
    dataFile.close(); //close file
    dataString = ""; //clear the string for the next line
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}
