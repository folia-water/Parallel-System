// Peristaltic Motor Test

#include <AFMotor.h>

AF_DCMotor motor(1);

void setup() {
  Serial.begin(9600); 
  delay(1000);
  // set up Serial library at 9600 bps
  Serial.println("Motor test!");

  // turn on motor for 100 seconds
  motor.setSpeed(255); //on full power
  motor.run(FORWARD);
  delay(100000);
  Serial.println("FINISHED");
  motor.run(RELEASE);
}

void loop() {
  
}
