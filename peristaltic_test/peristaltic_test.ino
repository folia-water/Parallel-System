// Peristaltic Motor Test
#define motorPin 11

void setup() {
  Serial.begin(9600); 
  delay(1000);
  
  Serial.println("Motor test!");

  // turn on motor for 100 seconds
  digitalWrite(motorPin, HIGH);
  delay(10000);
  Serial.println("FINISHED");
  digitalWrite(motorPin, LOW);
}

void loop() {
  
}
