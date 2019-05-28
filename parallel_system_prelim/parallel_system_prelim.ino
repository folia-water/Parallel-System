/*
  Parallel System setup with Arduino Mega
  LCD display connected to pins D48, D49, D50, D51, D52, D53
  Joystick with button pins A0, A1, D22
  Float Sensor pins D40
  Motor Output (now just an LED) D44
  Models behavior intended for final parallel System 
  Maps out panels and logic
  */
  
#include <LiquidCrystal.h> //use LCd

// initialize the library by associating any needed LCD interface pin
const int rs = 48, en = 49, d4 = 50, d5 = 51, d6 = 52, d7 = 53;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//some defs for pinouts
#define upDownSelect 1 //joystick y is A1
#define leftRightSelect 0 //joystick x is A0
#define selectPin 69 //joystick button is A15, D69 (port K) PCINT23
#define floatPin 40 //float sensor is on D22 for now
#define pumpPin 44 //LED output is D44 for now, soon to be motor

#define numFunnels 8
#define debounceTime 300  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

//main control vars
volatile boolean start = false; //for title screen vs. menus
volatile boolean drain = false; //are we draining

volatile boolean experimentOption = false; //did we start experimenting yet
volatile boolean funnelsSelect = false; //did we select a funnel yet
volatile boolean funnelsSelected = false; //were funnels chosen?
volatile boolean flowVolSelect = false; //did we select a flow volume yet
volatile boolean flowVolSelected = false; //were volumes chosen 
int flowVolumes[numFunnels+1];

volatile boolean experimentRun = false; //is it running?

volatile boolean washOption = false; //did we start washing yet
volatile boolean washRun = false; //run the wash cycle


volatile int displayState = 0; //which screen are we viewing?

//funnel and pump control vars eventually will go into class 
boolean filled = false; //is the funnel full yet?


void setup() {
  lcd.begin(16, 2); //lcd start
  delay(50);
  lcd.clear();
  Serial.begin(9600); //for debugging
  ADCSRA |= (0 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); //speeds up analog pins
  PCMSK2 |=  (1 << digitalPinToPCMSKbit(selectPin)); 
  // want A15/D69 digitalPinToPCMSKbit function for convenience
  //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
  //this triggers for joystick select pin, use PCMSK2 since PCINT23
  PCIFR  |= (1 << PCIF2);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE2); // enable interrupt for D16 to D23
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port K used here is (bit 2 of register) PCIE2
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  
}

void loop() {
  if (start) { //if started
    if (experimentOption) { //if in experiemnt mode
      experimentSetup(); //start the experiment setup function
    }else if (washOption) { //if in wash mode
      wash(); //start wash function
    }else{ //if neither
      displayState = changeValue(displayState, 1, 0, 1, leftRightSelect); //allow toggling of modes
      if (displayState == 0) { //display corresponding mode for each state
        lcd.print("Experiment Setup");
      }else if (displayState == 1) {
        lcd.print("Run Wash");
      }
    }
    if (drain) { //if we are draining
      drainSystem(); //drain the system
    }
  }else{ //otherwise if didnt start display startup screen
    lcd.print("Parallel System");
    lcd.setCursor(0,1);
    lcd.print("Press to Start");
  }
  delay(30);
  lcd.clear(); //clears lcd every cycle
}

void experimentSetup() {
  if (!flowVolSelect) { //if havent selected funnels yet
    lcd.print("Select Funnels");
    delay(1000);
    funnelsSelected = true; //and have chosen them
  }else if(funnelsSelected && flowVolSelect) { //once we have selected funnels
    //allow flow selection
    displayState = changeValue2(displayState, 1, 0, numFunnels, leftRightSelect); //allow toggling of modes
    if (displayState == 0) { //default screen
      lcd.print("Select Flow"); 
      lcd.setCursor(0,1);
      lcd.print("Volumes");
    }else{ //other display states
      //increment the display state by 1 to scroll thru the funnels
      //allow selection of flow volumes
      flowVolumes[displayState] = changeValue(flowVolumes[displayState], 200, 0, 4000, upDownSelect);
      //write everything to the screen
      lcd.clear();
      lcd.print("Funnel ");
      lcd.print(displayState);
      lcd.setCursor(0,1);
      lcd.print(flowVolumes[displayState]);
      lcd.print(" mL");
    }
    for (int i = 0; i <= numFunnels; i++) { //scroll thru all vlaues of array
      if (flowVolumes[i] != 0) { //if non zero flow volume, allow next stage
        flowVolSelected = true;
      }
    }
  }else if (flowVolSelected && !experimentRun) { //if vols were selected
    lcd.print("Push to Start");
  }else if (experimentRun) {
    experiment();
  }
}

void experiment() { //runs the experiment
  lcd.print("Starting Test");
  delay(2000);
  lcd.clear();
  lcd.print("Running...");
  delay(4000);
  reset();
}

void wash() {
  if (!washRun) { //if havent readied yet
    lcd.print("Load with EtOH");
    lcd.setCursor(0,1);
    lcd.print("Press when ready");
  }else{ //once we have, start the wash run
    lcd.print("Washing...");
    digitalWrite(pumpPin, HIGH);
    delay(2000);
    digitalWrite(pumpPin, LOW); //done washing now
    reset(); //reset states
  }
}

void drainSystem() {
  lcd.clear();
  lcd.print("Purging...");
  for (int i = 0; i < 5; i++) { //blink light
     digitalWrite(pumpPin, HIGH);
     delay(500);
     digitalWrite(pumpPin, LOW); //done washing now
  }
  reset(); //reset states
}

void reset() {
  start = false; //no longer started
  displayState = -1; //reset to default value
  drain = false; //not draining

  washOption = false; //no longer washing 
  washRun = false;

  experimentOption = false; //not running setup
  funnelsSelect = false; //nor are selecting funnels
  funnelsSelected = false; //no funnels were chosen
  flowVolSelect = false; //nor are selecting flow volumes
  flowVolSelected = false; //no volumes were chosen
  experimentRun = false; //not running an experiment
  
}

ISR (PCINT2_vect) { //when a button is pressed
  //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    PCMSK2 |=  (1 << digitalPinToPCMSKbit(selectPin));  //reenable interrupts on all pins in case they were off  
    if (!digitalRead(selectPin)) { //if the select button is pressed, 
      if (!start && !experimentOption && !washOption) {//and nothing happened
        //nested menus have delay times from debounce so that holding a button down doesnt immediately select the next item
        //requires capacitor on button to prevent spurious oscillations
        start = true; //we have started
        displayState = 0; //start on 1st screen
      }else{ //have started
        if (!experimentOption && !washOption) { //if started and on 1st display state
          if (displayState == 0) {
            experimentOption = true; //and pressed button, start experiment setup
          }else if (displayState == 1) { //if started and on 2nd display
           washOption  = true; //and pressed button, start wash cycle
          }
        }else if (experimentOption) { //if chosen experiment
          if(funnelsSelected && !flowVolSelect && !experimentRun) { //if in experiemnt setup and press button
            //if funnels are selectED, then can proceed otherwise dont let it
            flowVolSelect = true; //next stage is flow volume select
          }else if (funnelsSelected && flowVolSelected && !experimentRun) {//if in experiement setup and press button
            //if flow volumes were selectED then proceed, otherwise dont
            experimentRun = true; //start the experiment
          }
        }else if(washOption && !washRun) { //if in wash, and press button goto next stage
          //as long as last button push wasnt immediately before
           washRun = true; //start the wash cycle
        }
      } 
    }
   lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
}  



int changeValue(int value, int incrementAmount, int lowerBound, int upperBound, int pin) {
  //this function takes a value and increments or decrements it by said amount
  //using joystick control of up or down, or any analog control
  //holding joystick in max position for 1sec unlocks it and allows
  //quick movment up and down
  //but within bounding values
  static boolean lock; //this way its intialized once and stays local
  static boolean fastMode; //are we going in fast change mode?
  static uint32_t last_time; //when was the last time we changed the value
  
  int val = map(analogRead(pin),0,1023,0,6); //read the joystick
  if (val > 5) { //if joystick is up
    if (!lock) {//if no lock
      last_time = millis(); //store time we are changing value
      value += incrementAmount; //add to value
      if (value >= upperBound) { //set upper limit
        value = upperBound;
      }
      if (!fastMode) { //if we are not going fast
        lock = true; //keep the lock
      }else{ //if we are in fast mode
        lock = false; //disable it
      }
    }
    if (millis() - last_time > 1000) { //if last time changed value was more than a second
      lock = false; //allow value to change
      fastMode = true; //held for 1 sec, go fast
    }
  }
  if (val < 1) { //if its down
    if (!lock) {//if no lock
      last_time = millis(); //store time we are chainging value
      value -= incrementAmount; //decrease value
      if (value <= lowerBound) { //set lower limit of one
        value = lowerBound;
      }
      if (!fastMode) { //if we are not going fast
        lock = true; //keep the lock
      }else{ //if we are in fast mode
        lock = false; //disable it
      }
    }
    if (millis() - last_time > 1000) { //if last time changed value was more than a second
      lock = false; //allow value to change
      fastMode = true; //held for 1 sec, go fast
    }
  }
  if (val <= 5 && val >= 1) {
    lock = false; //when returned to middle position it resets lock
    fastMode = false; //if released go slow again
  }
  return value; //returns modified value
}

//uses a separate instance to avoid interaction when using both, due to static variables
int changeValue2(int value, int incrementAmount, int lowerBound, int upperBound, int pin) {
  //this function takes a value and increments or decrements it by said amount
  //using joystick control of up or down, or any analog control
  //holding joystick in max position for 1sec unlocks it and allows
  //quick movment up and down
  //but within bounding values
  static boolean lock2; //this way its intialized once and stays local
  static boolean fastMode2; //are we going in fast change mode?
  static uint32_t last_time2; //when was the last time we changed the value
  
  int val = map(analogRead(pin),0,1023,0,6); //read the joystick
  if (val > 4) { //if joystick is up
    if (!lock2) {//if no lock
      last_time2 = millis(); //store time we are changing value
      value += incrementAmount; //add to value
      if (value >= upperBound) { //set upper limit
        value = upperBound;
      }
      if (!fastMode2) { //if we are not going fast
        lock2 = true; //keep the lock
      }else{ //if we are in fast mode
        lock2 = false; //disable it
      }
    }
    if (millis() - last_time2 > 1000) { //if last time changed value was more than a second
      lock2 = false; //allow value to change
      fastMode2 = true; //held for 1 sec, go fast
    }
  }
  if (val < 2) { //if its down
    if (!lock2) {//if no lock
      last_time2 = millis(); //store time we are chainging value
      value -= incrementAmount; //decrease value
      if (value <= lowerBound) { //set lower limit of one
        value = lowerBound;
      }
      if (!fastMode2) { //if we are not going fast
        lock2 = true; //keep the lock
      }else{ //if we are in fast mode
        lock2 = false; //disable it
      }
    }
    if (millis() - last_time2 > 1000) { //if last time changed value was more than a second
      lock2 = false; //allow value to change
      fastMode2 = true; //held for 1 sec, go fast
    }
  }
  if (val <= 4 && val >= 2) {
    lock2 = false; //when returned to middle position it resets lock
    fastMode2 = false; //if released go slow again
  }
  return value; //returns modified value
}
