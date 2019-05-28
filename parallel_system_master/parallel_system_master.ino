 /*
  * Parallel System setup with Arduino Mega
  * LCD display connected to pins D38, D39, D40, D41, D42, D43
  * Joystick with button pins A0, A1, D12
  * LED buttons on pins D22-D29 for LEDs and
  * A8-A15 (D62-D69, PCINT 16-23) buttons
  * Solenoid Valves (modelled as LEDs) on D44-D49
  * Models behavior intended for final parallel System 
  * Maps out panels and logic as well as control structures
  * Serial communication with aux arduino on D18 and D19
  * Buzzer on D31
  * Also handles sending info to the cloud while experiment is running
  * and storing info to SD card via ethernet shield
  * on pins D50 (MISO),D51 (MOSI), D52 (SCK), D53 (SS, unused), D4 (ethernet SS), 
  * and D10 (SD SS)
  */
  
#include <LiquidCrystal.h> //use LCd

// initialize the library by associating any needed LCD interface pin
const int rs = 38, en = 39, d4 = 40, d5 = 41, d6 = 42, d7 = 43;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//some defs about the setup
#define numFunnels 8
#define numValves 6

//some defs for pinouts
#define upDownSelect 1 //joystick y is A1
#define leftRightSelect 0 //joystick x is A0
#define selectPin 12 //joystick button is D12, PCINT6
const int LEDpins[numFunnels+1] = {22, 23, 24, 25, 26, 27, 28, 29}; //pins for button LEDs, PORTA
const int valvePins[6] = {44, 45, 46, 47 ,48 ,49}; //pins for valves
const int buttonPins[numFunnels+1] = {62, 63, 64, 65, 66, 67, 68, 69}; //8 funnel buttons
#define buzzerPin 31 //for the beeper

#define debounceTime 100  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces
#define longPressTime 1000 //time for long presses to go back
volatile uint32_t longPress = 0;// to measure if long pressed

//main control vars
volatile boolean start = false; //for title screen vs. menus
volatile boolean drain = false; //are we draining
volatile int currentState; //for the back navigations

//for the different panels on setup
volatile boolean experimentOption = false; //did we start experimenting yet
volatile int expSetupPointer = 0; //int to track the menus
//0 is funnel select option, moves on after funnels are chosen
//1 is funnels selectED, funnels good, advances on button press
//2 is flow select option, moves on after flow vols are good
//3 is flow selectED, flows vols good, advances on button press
//4 is experiment ready to go, advances on button press
//5 is experiemnt running
volatile boolean funnelStates[numFunnels+1]; //is the funnel on or off
int funnelsActive; //are any funnel active?

int flowVolumes[numFunnels+1]; //flow volumes variable, leave some space in array

//vars for the experiment
volatile boolean experimentRun = false; //is it running?

//wash vars
volatile boolean washOption = false; //did we start washing yet
volatile int washPointer = 0; //int to track the menus
//0 is load with EtOH page
//1 is wash run

//other utility vars
volatile int displayState = 0; //which screen are we viewing?
volatile boolean settingsOption = false; //did we select settings?
volatile int settingsPointer = 0; //int to track menus
//0 is the main setting screen
//


void setup() {
  lcd.begin(16, 2); //lcd start
  Serial.begin(9600); //for debugging
  
  ADCSRA |= (0 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); //speeds up analog pins
  //sets the ADC clock
  
  PCMSK0 |=  (1 << digitalPinToPCMSKbit(selectPin)); 
  // want D12 digitalPinToPCMSKbit function for convenience
  //PCMSK0 register enables or disables pin change interrupts on bits selected, each bit is a pin  
  //this triggers for joystick select pin, use PCMSK0 since PCINT6
  PCIFR  |= (1 << PCIF0);  
  //clear any outstanding interrupts, writing 1 turns it off
  //the PCIFR is the flag register, flags are set when interrupt is triggered
  
  PCICR  |=  (1 << PCIE0); // enable interrupt for D2 to D13
  //main pin change interrupt register that enables or disables the interrupt register we want
  //pin change registers are grouped by port so port B used here is (bit 0 of register) PCIE0
  //port B. D50-D53, D10-D13  is bit 0 and D14,15, port J is bit 1  

  for (int i = 0; i <= numFunnels; i++) {
    flowVolumes[i] = 1000; //initalize the flow volumes to all start at 1000
    pinMode(LEDpins[i], OUTPUT); //sets i/o for buttons
    pinMode(buttonPins[i], INPUT); 
  }
  for (int i = 0; i <= 6; i++) { //set i/o for valves
    pinMode(valvePins[i], OUTPUT);
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
  if (start) { //if started interaction
    if (experimentOption) { //if in experiment mode
      experimentSetup(); //start the experiment setup function
    }else if (washOption) { //if in wash mode
      wash(); //start wash function
    }else if (settingsOption) { //if chose settings
      settings();
    }else{ //if none fo the above then still choosing
      expSetupPointer = 0;
      washPointer = 0;
      settingsPointer = 0;
      displayState = changeValue(displayState, 1, 0, 2, leftRightSelect); //allow toggling of modes
      switch(displayState) { //display corresponding mode for each state
        case 0:
          lcd.print("Experiment Setup");
          break;
        case 1:
          lcd.print("Run Wash");
          break;
        case 2:
          lcd.print("Settings");
          break;
        default: //just in case it gets out fo bounds, go back to the start
          reset();
          break;
      }
    }
    //if (drain) { //if we are draining
      //drainSystem(); //drain the system
    //}
  }else{ //otherwise if didnt start display startup screen
    lcd.print("Parallel System");
    lcd.setCursor(0,1);
    lcd.print("Press to Start");
  }
  //check for long presses and go back if needed
  if (millis() - longPress > longPressTime && !digitalRead(selectPin)) {
    back(); //if held the button down for 1 sec go back
  }
  delay(30); //refresh rate
  lcd.clear(); //clears lcd every cycle
}


ISR (PCINT0_vect) { //when a button is pressed within PORT B
  //need to handle which button is pressed, this is only the joystick button
  //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    PCMSK0 |=  (1 << digitalPinToPCMSKbit(selectPin));  //reenable interrupts on all pins in case they were off  
    
    if (!digitalRead(selectPin)) { //if the select button is pressed, 
      longPress = millis(); //start time as soon as button is pressed 
      if (!start) {//and didnt start yet
        //nested menus have delay times from debounce so that holding a button down doesnt immediately select the next item
        //requires capacitor on button to prevent spurious oscillations
        start = true; //we have started
        displayState = 0; //start on 1st screen
      }else{ //have started
        if (!experimentOption && !washOption && !settingsOption) { //if started and choosing
          switch(displayState) { //display corresponding mode for each state
            case 0:
              experimentOption = true; //selected the experiment option
              break;
            case 1:
              washOption = true; //wash option selected
              break;
            case 2:
              settingsOption = true; //settings selected
              break;
            default: //just in case it gets out of bounds, go back to the start
              reset();
              break;
          }
        }else if (experimentOption) { //if chosen experiment
          // Serial.println(expSetupPointer);
          currentState = expSetupPointer; //get the current state
          switch (expSetupPointer) { //upon button push within the exp setup
            case 1: // have pressed button after funnel select
              expSetupPointer = 2; //so move on to flow select
              break;
            case 3: //flow vols were all good and have pressed button
              expSetupPointer = 4; //so move on to exp ready screen
              break;
            case 4: //exp ready and haave pushed button
              expSetupPointer = 5; //so run exp
              break;
          }
        }else if(washOption) { //if in wash, and press button goto next stage
          currentState = washPointer; //get the current wash pointer state
          washPointer = 1; //start the wash cycle
        }else if (settingsOption) { //if chose settings
          currentState = settingsPointer; //get the current menu screen
        }
      } 
    } //end select button if statements
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
}  

ISR (PCINT2_vect) { //when a button is pressed within the funnels PORT K
  //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) {
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(buttonPins[i])); 
      // want A8-A15 digitalPinToPCMSKbit function for convenience
      //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
    }  
    for (int i = 0; i <= numFunnels; i++) {
      if (digitalRead(buttonPins[i])) { //if that button is pushed
        funnelStates[i] = !funnelStates[i];
        digitalWrite(LEDpins[i], HIGH); //light up that LED
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
}  

void experimentSetup() {
  if (expSetupPointer == 0 || expSetupPointer == 1) { //if havent selected funnels yet
    lcd.print("Select Funnels"); //are currently choosing them
    for (int i = 0; i <= numFunnels; i++) {  //choosing funnels routine
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(buttonPins[i])); //enable interrupts for buttons
      if (funnelStates[i]) digitalWrite(LEDpins[i], HIGH); //if a funnel is on light up that LED
      else digitalWrite(LEDpins[i], LOW); //otherwise turn it off
    }
    funnelsActive = PINA; //read LED pins, if none are active then
    if (funnelsActive == 0) expSetupPointer = 0; //no funnels active then cant advance yet
    else expSetupPointer = 1; //otherwise some funnel is chosen and can advance
  }else if(expSetupPointer == 2 || expSetupPointer == 3) { //once we have selected funnels
    //and pressed to goto the flow vol select
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
        expSetupPointer = 3;
      }
    }
  }else if (expSetupPointer == 4) { //if vols were selected and everything else too
    lcd.print("Push to Start");
  }else if (expSetupPointer == 5) { //have pushed so start the experiment
    experiment();
  }
}

void experiment() { //runs the experiment
  lcd.print("Starting Test");
  delay(2000);
  lcd.clear();
  lcd.print("Running...");
  //add stuff here to do exepriment
  
  delay(4000);
  reset();
}

void wash() {
  if (washPointer == 0) { //if havent readied yet
    lcd.print("Load with EtOH");
    lcd.setCursor(0,1);
    lcd.print("Press when ready");
  }else if (washPointer == 1) { //once we have, start the wash run
    lcd.print("Washing...");
    //add stuff here to start a wash cycle
    
    delay(2000);
    reset(); //reset states
  }
}

void settings() {
  //settings options
}

void drainSystem() {
  lcd.clear();
  lcd.print("Purging...");
  delay(2000);
  reset(); //reset states
}

void back() { //goes back within the menus
  if (experimentOption) { //if in experiment mode
    switch (currentState) {
      case 0: //if in the select funnels menu
        for (int i = 0; i <= numFunnels; i++) {
          funnelStates[i] = false; //reset the funnels
          digitalWrite(LEDpins[i], LOW); //turn off all LEDs
          PCMSK2 |=  (0 << digitalPinToPCMSKbit(buttonPins[i])); //turn off button interrupts
        }
        experimentOption = false;
        break;
      case 1: //if funnels have been selected already
        for (int i = 0; i <= numFunnels; i++) {
          funnelStates[i] = false; //reset the funnels
          digitalWrite(LEDpins[i], LOW); //turn off all LEDs
          PCMSK2 |=  (0 << digitalPinToPCMSKbit(buttonPins[i])); //turn off button interrupts
        }
        experimentOption = false;
        break;
      case 2: //if in flow vol select
        expSetupPointer = 0; //back to funnel selection
        break;
      case 3: //if flow vol have been selected
        expSetupPointer = 0; //back to funnel selection
        break;
      case 4: //if in ready to start menu
        expSetupPointer = 3; //back to flow vol select
        break;
    }
  }else if (washOption) { //if in wash mode
    washPointer -= 2;
    if (washPointer <= 0) { //lower bound
      washPointer = 0;
    }
  }else if (settingsOption) { //if chose settings
    switch (currentState) {
      case 0: //if in the settings menu
        settingsOption = false; //go back
        break;
    }
  }
}

void reset() { //resets all the vars to their default states
  start = false; //no longer started
  displayState = -1; //reset to default value
  drain = false; //not draining

  washOption = false; //no longer washing 
  washPointer = 0;

  experimentOption = false; //not running setup
  expSetupPointer = 0; //back at the satrt of the menus
  for (int i = 0; i <= numFunnels; i++) {
    funnelStates[i] = false; //reset the funnels
    digitalWrite(LEDpins[i], LOW); //turn off all LEDs
    PCMSK2 |=  (0 << digitalPinToPCMSKbit(buttonPins[i])); //turn off button interrupts
  }

  settingsOption = false; //not in the settings menu
  settingsPointer = 0; //and those nested menus restart
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
  
  int val = map(analogRead(pin),0,1023,6,0); //read the joystick
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
  
  int val = map(analogRead(pin),0,1023,6,0); //read the joystick
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
