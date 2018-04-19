// Train_Fns_Vars_Global declares and defines all functions and variables that are global to all (or nearly all) Arduino modules.
// Rev: 11/05/17

#include <Train_Fns_Vars_Global.h>

// Pass address of serial port to use (0..3) such as &Serial1, and baud rate such as 9600 or 115200.
//Display_2004 LCD2004(&Serial1, 115200);  // Instantiate 2004 LCD display "LCD2004."
//Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.


//byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
//byte RS485MsgOutgoing[RS485_MAX_LEN];

//char lcdString[LCD_WIDTH + 1];                   // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.


/*  Each module needs its own version since LEG and SWT need to init shift registers, which are not objects in some other modules
void endWithFlashingLED(int numFlashes) {
  if (THIS_MODULE == ARDUINO_LEG) {
    requestEmergencyStop();  // Stop all Legacy devices esp. locomotives that might be moving
  }
  if ((THIS_MODULE == ARDUINO_LEG) || (THIS_MODULE == ARDUINO_SWT)) {
    initializeShiftRegisterPins();  // Release relays holding accessories or turnout coils
  }
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
*/
