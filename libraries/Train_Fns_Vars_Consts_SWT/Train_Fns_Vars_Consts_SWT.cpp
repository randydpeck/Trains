// Train_Fns_Vars_Consts_SWT contains all functions, variables, and constants that are unique to A_SWT.
// It is also a child class to parent class(es) that contain fns/vars/consts that are used by more than one module.
// Rev: 11/06/17

#include <Train_Fns_Vars_Consts_SWT.h>

/*
const byte THIS_MODULE = ARDUINO_SWT;

void checkIfHaltPinPulledLow() {
  // Rev 10/29/16
  // Check to see if another Arduino, or Emergency Stop button, pulled the HALT pin low.
  // If we get a false HALT due to turnout solenoid EMF, it only lasts for about 8 microseconds.
  // So we'll sit in a loop and check twice to confirm if it's a real halt or not.
  // Updated 10/25/16 to increase EMF spike delay from 25 microseconds to 50 microseconds, because
  // A-MAS was tripped once, presumably by a "double" spike.
  // Rev 10/19/16 extend delay from 50 microseconds to 1 millisecond to try to eliminate false trips.
  // No big deal because this only happens very rarely.
  if (digitalRead(PIN_HALT) == LOW) {   // See if it lasts a while
    delay(1);  // Pause to let a spike resolve
    if (digitalRead(PIN_HALT) == LOW) {  // If still low, we have a legit Halt, not a neg voltage spike
      initializeShiftRegisterPins();   // This will disable all relays/turnout solenoids.  VERY IMPORTANT!
      wdt_disable();  // No longer need WDT since relays are released and we are terminating program here.
      chirp();
      sprintf(lcdString, "%.20s", "HALT pin low!  End.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      while (true) {}  // For a real halt, just loop forever.
    }
    else {
      sprintf(lcdString, "False HALT detected.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}

void endWithFlashingLED(int numFlashes) {
  initializeShiftRegisterPins();  // Release relays holding accessories or turnout coils (used by A_SWT and A_LEG)
  while (true) {
    for (int i = 1; i <= numFlashes; i++) {
      digitalWrite(PIN_LED, HIGH);
      chirp();
      digitalWrite(PIN_LED, LOW);
      delay(100);
    }
    delay(1000);
  }
  return;  // Will never get here due to above infinite loop
}

void initializeShiftRegisterPins() {
  // Rev 9/14/16: Set all chips on both Centipede shift registers to output, high (i.e. turn off relays on relay boards)
  for (int i = 0; i < 8; i++) {         // For each of 4 chips per board / 2 boards = 8 chips @ 16 bits/chip
                                        // Set outputs high before setting pin mode to output, to prevent brief low being sent to Centipede shift register outputs
    shiftRegister.portWrite(i, 0b1111111111111111);  // Set all outputs HIGH (pull LOW to turn on a relay)
    shiftRegister.portMode(i, 0b0000000000000000);   // Set all pins on this chip to OUTPUT
  }
  return;
}
*/