// Train_Fns_Vars_Global declares and defines all functions and variables that are global to all (or nearly all) Arduino modules.
// Rev: 11/05/17

#include <Train_Fns_Vars_Global.h>

char lcdString[21];                   // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.



// IMPORTANT: THIS IS A DIFFERENT VERSION THAN MOST MODULES DUE TO THE INIT SHIFT REGISTER, so I'll need to figure out how to deal with that.
void endWithFlashingLED(int numFlashes) {
  // Rev 10/05/16: Version for Arduinos WITH relays that should be released (A-SWT turnouts, A-LEG accessories)
  initializeShiftRegisterPins();  // Release all relay coils that might be activating turnout solenoids
  requestEmergencyStop();
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

void chirp() {
  // Rev 10/05/16
  digitalWrite(PIN_SPEAKER, LOW);  // turn on piezo
  delay(10);
  digitalWrite(PIN_SPEAKER, HIGH);
  return;
}

void requestEmergencyStop() {
  // Rev 10/05/16
  pinMode(PIN_HALT, OUTPUT);
  digitalWrite(PIN_HALT, LOW);   // Pulling this low tells all Arduinos to HALT (including A-LEG)
  delay(1000);
  digitalWrite(PIN_HALT, HIGH);  
  pinMode(PIN_HALT, INPUT);
  return;
}
