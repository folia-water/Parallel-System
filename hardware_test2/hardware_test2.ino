/* Sketch to test all the hardware setup, as described in schematic diagram. 
 *  TEsts motors, mocked as leds and float sensors on aux arduino setup
 */


#define numFunnels 8 //pinout definitions
const int motorPins[numFunnels+1] = {11, 9, 8, 7, 6, 5, 3, 2};
const int floatPins[numFunnels+1] = {62, 63, 64, 65, 66, 67, 68, 69};
volatile boolean floatStates[numFunnels+1]; //for storing float positions
volatile boolean floatTriggered = false; //is a float activated
volatile int inputStates;

#define debounceTime 30  //time for button debounces
volatile uint32_t lastInterrupt = 0; //used to measure button debounces

void setup() {
  Serial.begin(9600); //for debugging
  for (int i = 0; i <= numFunnels; i++) { //set i/o pins
    pinMode(motorPins[i], OUTPUT);
    pinMode(floatPins[i], INPUT);
  }
  for (int i = 0; i <= numFunnels; i++) {
    PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
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

}

void loop() {
  for (int i = 0; i <= numFunnels; i++) { //scroll thru each pin and light up led
    if (!floatTriggered) { //if no float is triggere scroll
      for (int j = 0; j <= 255; j++) { //fade in and out
        analogWrite(motorPins[i], j);
        delay(1);
      }
      for (int j = 255; j >= 0; j--) {
        analogWrite(motorPins[i], j);
        delay(1);
      }
      delay(50);
    }else{ //if float triggered
      if (floatStates[i]) { //if that float was activated
        digitalWrite(motorPins[i], HIGH); //light up that LED/motor
      }else{
        digitalWrite(motorPins[i], LOW);
      }
    }
    inputStates = PINK; //need to check during the loop in case has been reset
    if (inputStates == 255) { //if the entire port is high (default state)
      floatTriggered = false; //then a float hasnt been triggered
    }else{
      floatTriggered = true;
    }
  }
}

ISR (PCINT2_vect) { //when a button is pressed
  //need to handle which button is pressed
 //debounce
  uint32_t interrupt_time = millis(); //store time state when interrupt triggered
  if (interrupt_time - lastInterrupt > debounceTime) { //if the last time triggered is more than debounce time before
    for (int i = 0; i <= numFunnels; i++) {
      PCMSK2 |=  (1 << digitalPinToPCMSKbit(floatPins[i])); 
      // want A8-A15 digitalPinToPCMSKbit function for convenience
      //PCMSK2 register enables or disables pin change interrupts on bits selected, each bit is a pin  
    }
    inputStates = PINK;
    if (inputStates == 255) { //if the entire port is high (default state)
      floatTriggered = false; //then a float hasnt been triggered
    }else{
      floatTriggered = true;
    }
    for (int i = 0; i <= numFunnels; i++) {
      if (!digitalRead(floatPins[i])) { //if that float is pushed
        floatStates[i] = true; //this float is on
        digitalWrite(motorPins[i], HIGH); //turn on that led
      }else{
        floatStates[i] = false;
        digitalWrite(motorPins[i], LOW);
      }
    }
    lastInterrupt = interrupt_time; //store interrupt trigger for next time
  }
}  
