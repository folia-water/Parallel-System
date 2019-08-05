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
int numFunnels = 4; //how many funnels are we testing
const int numFunnelsTotal = 8; //the setup max amount

//pinout definitions arrays start at 0 goto numFunnels, funnels are started at 1 so 1st entry is dummy
const int motorPins[numFunnelsTotal+1] = {-1, 11, 9, 8, 7, 6, 5, 3, 2}; //the pinouts of the setup
const int floatPins[numFunnelsTotal+1] = {-1, 62, 63, 64, 65, 66, 67, 68, 69};
//use slightly larger arrays to avoid going out of bounds

#define startPin 12
#define activeLED 13
#define bypassPin 23 //for bypassing the samples

#define buzzerPin 22 //buzzer output for sample done

#define chipSelect 4 //chip select pin for SD card

volatile boolean start = false; //did we start yet?
volatile boolean purge = false; //are we purging the system?

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

//vars for determining which float is triggered 
volatile boolean floatStates[numFunnelsTotal+1]; //for storing float positions

//vars for pumps
volatile float influentVols[numFunnelsTotal+1]; //for storing volume going in
volatile uint32_t pumpTimes[numFunnelsTotal+1]; //for motor run times
volatile uint32_t checkVolume; //are we reading the volume of the influent
volatile uint32_t startTime; //waht time did the run start?
volatile float currentTimeHolder; //for storing the current time while paused

//each motor has its own flow rate, starts at motor 1
const float motorFlowRate[numFunnelsTotal+1] = {0, 
14.55,  //flow rates calculated for each motor based on 20 sec tests
13.49, 
16.79,
19.91, //updated 1st 4 flow rates based on runs done
18.46,
16.73,
12.56,
11.44}; //flow rate of motors in mL/s

const float checkInfluentVol = 1.0; //update time to check the influent vol of the motor
const int sampleVol = 550; //sample collection at volume total of 600 ml
int maxInfluentVol = 1000; //for the 1 L of sample
const int bypassVolume = 4000; //max volume allowed for bypassed runs

volatile boolean pauseTime = false; //were we paused?
volatile boolean sample[numFunnelsTotal+1]; //did we take samples?
boolean flasher; //for flashing the LED, will be replaced by timers later
volatile boolean hasTaken[numFunnelsTotal+1]; //have we taken all the samples yet?
volatile boolean finished[numFunnelsTotal+1]; //have we finished yet?
volatile boolean manual; //did we start from a manual run?

uint32_t previousSave; //for ctracking the interval for saving the files
const int saveTime = 2; //interval to save data log file in mins
float currentTime; //for datalogging tracking the current time
String dataString = ""; //for writing to the datalog file
char serialIn;

void setup() {
  Serial.begin(115200); //for debugging
  delay(250);
  Serial.println('A'); //transmit request code to connect
  //Serial.print("Initializing SD card..."); //setup SD card debugging message

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    while (1) { //just send fail codes
      Serial.println('F'); //fail code
      delay(500);
    }
  }
  //Serial.println("card initialized."); //debug message
  //i/o
  pinMode(startPin, INPUT); //for button
  pinMode(activeLED, OUTPUT); //led
  pinMode(buzzerPin, OUTPUT);//buzzer

  for (int i = 0; i <= numFunnelsTotal; i++) {
    pinMode(motorPins[i], OUTPUT); //set i/o
    pinMode(floatPins[i], INPUT);
  }

  PCIFR  |= (1 << PCIF2);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  PCICR  |=  (1 << PCIE2); // enable interrupt for floats
  PCIFR  |= (1 << PCIF0);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  PCICR  |=  (1 << PCIE0); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  
  PCMSK0 |=  (1 << digitalPinToPCMSKbit(startPin)); //enable start button in case

}

void loop() {
  if (start) { //if button was pushed and run has begun
    for (int i = 0; i <= numFunnels; i++) { 
      if (floatStates[i] || sample[i] || finished[i]) digitalWrite(motorPins[i], LOW); //if that float was activated turn off the motor to not overflow
      //or if that funnel is at a sample point
      else digitalWrite(motorPins[i], HIGH); //otherwise this pump is on
      //only if the float isnt, and they havent passed sample points
      checkSamples(); //check to see if sample inlfuents has been reached yet
      if (digitalRead(bypassPin) || serialIn == 'B') setBypass();
      //if any sample is ready allow reset
     
    } //end for loop
  }else if (purge) { //otherwise if purging
    for (int i = 0; i <= numFunnels; i++) { //scroll thru the number of funnels
      if (floatStates[i]) digitalWrite(motorPins[i], LOW); //if that float was activated turn off the motor to not overflow
      //or if that funnel is at a sample point
      else digitalWrite(motorPins[i], HIGH); //otherwise this pump is on
    }
  }else{ //if not started nor purging
     if (digitalRead(startPin) || serialIn == 'S') { //when pressed start
      if (digitalRead(startPin)) manual = true; //if started via manual button press
      start = true;
      initializeRun(); //start a run
      delay(10); //wait for previous data to send
      writeSD("Start of run"); //title
      writeSD(" "); //one line space
      if (manual)  dataString += "Manual Run"; //if started from button push
      else dataString += readStartTime(); //get the start time
      writeSD(dataString); //save the start time
      dataString += "Time,        "; //add space to data string
      for (int i = 1; i <= numFunnels; i++) { //set header
        dataString += String(i); //create a header of numbers in the data file
        dataString += ",        ";
      }
      writeSD(dataString); //write to SD
      writeSD(" "); //add a line space
      startTime = millis(); //save the start time to zero out the run
      for (int i = 1; i <= numFunnels; i++) { //scroll thru eachfunnel
        pumpTimes[i] = millis(); //start time of pump interval for the next round
      }
    }//end of start sequence
    for (int i = 0; i <= numFunnelsTotal; i ++) { //for safety when not doing anything turn off motors
      digitalWrite(motorPins[i], LOW);
    }
  }
}

void serialEvent() { //runs every loop when serial data is recieved
  // get the new byte as soon as they are available
  serialIn = (char)Serial.read();
  if (serialIn == 'X') reset(); //stop code to terminate run
  if (serialIn == 'P') { //for purging the system
    purge = true; //are purging
    initializeRun(); //start the run, setting leds and interrupts
    //no volume measurements and such since just purging
  }
  for (int i = 0; i <= numFunnels; i++) {
   if (sample[i] && serialIn == 'C') { //if command was sent and sample was read
     delay(10);
     int index = Serial.read(); //get the button that was pressed
     pumpTimes[index] = millis(); //update that pump time and start it again
     digitalWrite(motorPins[index], HIGH); //turn motor back on
     hasTaken[index] = true; //have now taken samples from these funnels
     sample[index] = false; //only reset that particular sample
    }
  }
  if (serialIn == 'A') Serial.println('A'); //if receive a confirm for conenct code
     //a simple response to a connection request
  if (serialIn == 'R') { //for resuming a run
    start = true; //now started again
    initializeRun(); //start the run, setting leds and interrupts
    delay(120); //wait for the string to be sent
    String data[numFunnels+1]; //some local vars for reading incoming data
    static char inChar;
    static int index;
    //terminated by E character and separated by comma
    inChar = Serial.read();
    while (inChar != 'E') { //until the incoming byte is an E, read string
      inChar = Serial.read();
      data[index] += inChar; // add character to the string array
      if (inChar == ',') { //if delimiter
        index++; //onto the next entry
      }
      delay(1);
    } //end while loop for reading data
    for (int i = 1; i <= numFunnels; i++) { //scroll thru eachfunnel
      pumpTimes[i] = millis(); //start time of pump interval for the next round
    } //update the start time of the run since it was paused 
    startTime = millis(); //the new start time since the run was stopped
    currentTimeHolder = data[0].toFloat(); //also update the current time from data
    //use the holder since will add to it later
    pauseTime = true; //need to add the pause time offset
    for (int i = 0; i <= numFunnels; i++) {
      influentVols[i] = data[i].toFloat(); //convert strings to floats
    }
    //writeSD("paused"); debug message
  }
}

ISR (PCINT2_vect) { //when a button is pressed
 //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) { //need to start loop 1 entry before to allow proper update on 1st funnel
      if ((digitalRead(floatPins[i])) && (start || purge)) { //if that float is pushed
        //Serial.print("Float: "); //some debug messages
        //Serial.print(i);
        //Serial.println(" is on");
        if (i >= 1)  getInfluentVols(i); //append to influent total since motor has just turned off
        //only get influents on the actual motors, ot the 0th dummy one
        floatStates[i] = true; //this float is on
        digitalWrite(motorPins[i], LOW); //turn off that motor
      }else{
        //Serial.print("Float: "); //some debug messages
        //Serial.print(i);
        //Serial.println(" is off ");
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
      if (sample[i] && !finished[i]) { //if that sample was reached, have cleared it now, and have taken the sample
        hasTaken[i] = true; //have now taken samples from these funnels
        sample[i] = false; //only reset that particular sample
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
} 

void initializeRun() { //starts data log, creates header and such
  delay(10); //wait a sec for data to be present
  if (manual) numFunnels = numFunnelsTotal; //use the max number of funnels if manual run
  else numFunnels = Serial.read(); //get number of funnels
  digitalWrite(activeLED, HIGH); //turn on active led
  for (int i = 0; i <= numFunnels; i++) { //enables interrupts for floats
    PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
    // want A8-A15 digitalPinToPCMSKbit function for convenience
    //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
  }
}

String readStartTime() { //reads the incoming start time and returns it as a string
  String data;
  char inChar;
  //terminated by E character
  inChar = Serial.read();
  while (inChar != 'E') { //until the incoming byte is an E, read string
    inChar = Serial.read();
    data += inChar; //add char to the string
    delay(1);
  }
  return data; //return the string
}

void reset() { //stops run and resets for a new one
  start = false; //if the back button was pressed stop the run
  purge = false; //mnot doign anything
  pauseTime = false; //not previously paused
  manual = false; //reset run so didnt start it manually
  digitalWrite(activeLED, LOW); //reset the LED
  //PCICR= 0x00; // disable interrupt for floats
  for (int j = 0; j <= numFunnels; j++) {
    PCMSK2 |=  (0 << digitalPinToPCMSKbit(floatPins[j])); 
    digitalWrite(motorPins[j], LOW); //turn off motors and disable float interrupts
    hasTaken[j] = false; //all samples reset
    sample[j] = false;
    finished[j] = false; //samples arent done
    influentVols[j] = 0.0; //reset influent vols
  }
}

void checkSamples() {
  if (float(millis() - checkVolume)/1000 >= checkInfluentVol) { //if the time has elapsed, check the volume
    checkVolume = millis(); //store time of last check of volume
    //if didnt pause and just normal operation
    /*String writestuff = "data: current";
    writestuff += String(currentTime);
    writestuff += " starttime: ";
    writestuff += String(startTime);
    writestuff += " millis: ";
    writestuff += String(millis()); //for debugging time values
    writeSD(writestuff);*/
    currentTime += float((millis() - (currentTime*1000)) - startTime)/1000; //the delta t added gives current time
    if (pauseTime) { //if need to update the time since was started from a pause
      currentTime += currentTimeHolder; //after time is calcaulted, add to it from the paused time offset
    }
    //subtracting the start time of the program since that represents time = 0
    //add to the string so can add to our text file
    dataString = ""; //clear data string to start a new line
    dataString += String(currentTime); //on the data file start with the time on left most column
    dataString += ",    ";
    for (int i = 1; i <= numFunnels; i++) {
      getInfluentVols(i); //print the data of volume inputted
      //read all floats if all active for the first time, then activate sample
      if (influentVols[i] >= sampleVol && !sample[i]  && !hasTaken[i]) { //if the volume is at the measure point
        //and we havent already taken a sample and not in sampling mode
        digitalWrite(motorPins[i], LOW); //stop that motor
        sample[i] = true; //have now taking sample on that funnel
        Serial.print('D'); //indicator character that a sample is done
        Serial.write(i); //send which sample finished
        Serial.println(); //line feed signals end of data
        PCMSK0 |=  (1 << digitalPinToPCMSKbit(startPin)); //for the start button enable sample registered
        beep(); //to alert sample
      } //if not sampling continue running as normal

      if (influentVols[i] >= maxInfluentVol && hasTaken[i]) { //if reached the end of the run
        finished[i] = true; //stop the motor
        Serial.print('L'); //indcator character that a sample is done
        Serial.write(i); //send which sample finished
        Serial.println(); //line feed signals end of data
        digitalWrite(motorPins[i], LOW); 
        //just wait until others have finished
      }
    }
    dataString += "d"; //end stop character of end of data rather than commands
    Serial.flush(); //wait until and outgoing data is finished
    Serial.println(dataString); //print whats being written to Serial
    writeSD(dataString); //only after all 8 funnels have been written to the string
  }
}

void getInfluentVols(int index) { //tracks volume added for pump delinated by index
  dataString += String(influentVols[index]); //then influent vol;
  dataString += ",    "; // space between funnels
  if (!floatStates[index] && !sample[index] && !finished[index]) { //as long as motor is on i.e. float not active
    //or the motor isnt stopped awaitng other funnels to catch up and sample
    //current time minus the last pump active time times the average flow rate of the motor gives total volume
    influentVols[index] += ((float(millis() - pumpTimes[index])/1000)*motorFlowRate[index]); //calculate change in volume
    pumpTimes[index] = millis(); //start time of pump interval for the next round
    //convert to the time to secs
  }
}

void setBypass() { //set vars for bypass 4 L run
  serialIn = ""; //so it doesnt constantly trigger
  beep(); //to acknowledge that bypass run
  for (int j = 0; j <= numFunnels; j++) {
    //sample[j] = true; //bypass the sampling to allow automated dispensing
    hasTaken[j] = true; //set appropraite booleans to bypass sampling
  }
  maxInfluentVol = bypassVolume; //if pressed for pure water run now
}
  

void beep() { //beeps the buzzer and accomadates stopping the volume measurments
  //uses timer 4 to get an accurate 1 sec time to beep to not occupy the main loop
  //uses CTC mode to clear timer on compare match and set value for top value
  //allows tuning for the 1 sec time interval
  TCCR4A = 0x00; //clear the tccra register, not using any bits in it
  TCCR4B = 0x00; //clear the tccrb register, sets WGM32 for CTC mode
  //sets the clock select to 1024 for 1 Hz
  TCCR4B |= (1 << WGM42) | (1 << CS42) | (1 << CS40);
  TCNT4 = 0; //reset the timer count
  OCR4A = 15624; //value for 1 sec interval
  TIMSK4 |= (1 << OCIE4A); //enable compare match for OCR3A register
  digitalWrite(buzzerPin, HIGH); //beep briefly to indicate sample
}

ISR (TIMER4_COMPA_vect) { //triggered when timer 3 hits value
  digitalWrite(buzzerPin, LOW);
}

void writeSD(String input) { //function takes a string and writes it to one line of a text file on an SD card
  File dataFile = SD.open("MASTER.csv", FILE_WRITE); //open and create file object save as a csv
  File logFile = SD.open("datalog.csv", FILE_WRITE); //create a datalog that is managable
  // if the files are available, write to them:
  if (dataFile && logFile) {
    dataFile.println(input);//prinbt with new lines
    dataFile.close(); //close file
    if (millis() - previousSave > (saveTime*60*1000)) { //convert minutes to milliseconds
       logFile.println(input);//print with new lines
       logFile.close(); //close file
       previousSave = millis(); //update the save time
    }
    dataString = ""; //clear the string for the next line
  }else{ //if not available
    reset(); //stop the run and reset
    while(1) {
      Serial.println('F'); //fail codes
      delay(300);
    }
  }
}
