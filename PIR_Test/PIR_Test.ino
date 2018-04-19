// Rev: 12-27-17
// Test the "TENSTAR ROBOT" PIR motion sensor module

int ledPin = 13;  // LED on Pin 13 of Arduino
int pirPin = 8; // Input for HC-S501

int pirValue; // Place to store read PIR Value


void setup() {
  
  pinMode(ledPin, OUTPUT);
  pinMode(pirPin, INPUT);
 
  digitalWrite(ledPin, LOW);
  
  
  
}

void loop() {
  pirValue = digitalRead(pirPin);
  digitalWrite(ledPin, pirValue);

}
