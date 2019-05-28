/* Parallel System setup with Arduino Mega, aux board 
 *  controls the pumps and floats and handles the experiement running
 *  Floats on A8-A15 (D62-D69, PCINT16-23)
 *  Motors, (modelled as LEDs here) on D2, D3, D5, D6, D7, D8, D9, D11
 *  Skips D4 and D10 for option expansion via ethernet or SD card later on, left open to support more channels
 *  calculates and sends data back to master arduino for cloud storage and analysis
 *  
 */


//some defs about the setup
#define numFunnels 8

//pinout definitions
const int motorPins[numFunnels+1] = {11, 9, 8, 7, 6, 5, 3, 2}; //the pinouts of the setup
const int floatPins[numFunnels+1] = {62, 63, 64, 65, 66, 67, 68, 69};

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

//vars for detemrining which float is triggered 
volatile boolean floatStates[numFunnels+1]; //for storing float positions
volatile boolean floatTriggered = false; //is a float activated
volatile int inputStates;

//vars for pumps
volatile int influentVols[numFunnels+1]; //for storing volume going in
volatile uint32_t pumpTimes[numFunnels+1]; //for motor run times
volatile boolean checkVolume = false; //are we reading the volume of the influent
const float motorFlowRate = 0.15; //flow rate of motors in mL/min

void setup() {
  Serial.begin(9600); //for debugging
  
  for (int i = 0; i <= numFunnels; i++) { //set i/o pins
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
  for (int i = 0; i <= numFunnels; i++) { //scroll thru each pin and light up led
    if (floatStates[i]) digitalWrite(motorPins[i], LOW); //if that float was activated turn off the motor to not overflow
    else digitalWrite(motorPins[i], HIGH); //otherwise this pump is on
    getInfluentVols(i); //track fluid addition
    
    delay(300); //slow for debugging
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
    for (int i = 0; i <= numFunnels; i++) {
      if (!digitalRead(floatPins[i])) { //if that float is pushed
        floatStates[i] = true; //this float is on
        digitalWrite(motorPins[i], HIGH); //turn on that motor
      }else{
        floatStates[i] = false; //that float is off, turn off the motor
        digitalWrite(motorPins[i], LOW);
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
} 

void getInfluentVols(int index) { //runs the motors and tracks volume added for each
  checkVolume = !checkVolume;
  if (checkVolume && !floatStates[index]) { //if time to check the volume and float isnt active
    influentVols[index] += (millis() - pumpTimes[index])*motorFlowRate; //calculate chnage in volume
    pumpTimes[index] = millis(); //update pump time for next cycle
  }
  Serial.print(index);
  Serial.print(" ");
  Serial.println(influentVols[index]);
}
