#include <Train_Consts_Global.h>
char APPVERSION[LCD_WIDTH + 1] = "A-BTN Rev. 07/11/18";
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?

// A_BTN watches for control panel pushbutton presses by operator, and transmits that info to A_MAS.
// This is an "OUTPUT-ONLY" module that does not monitor or respond to any other Arduino (it does watch for A-MAS mode changes.)
// A_BTN is only active when the system is in Manual mode, Running state.  Turnout button presses are ignored during all other
// modes and states.  P.O.V. support can be added later if relevant.

// 04/22/18: Updating new global const, message and LCD display classes.
// 09/16/17: Brought a lot of loop code out to functions; much streamlining, variable re-naming.
// 09/13/17: Changed variable suffixes and constant names for better programming form.
// 08/28/17: Const LCD width, converted some inline loop code to functions, added RS485 protocol comments, and other clean-up.
// 11/01/16: InitializeShiftRegisterPins() and delay in LCD.send()
// 11/01/16: updated "delay between button release and next press" logic.
// 11/01/16: ALSO was getting RS485 input buf overflow if I started and stopped Manual mode constantly and pressed a ton of buttons constantly.
// I think that problem is fixed.  Regardless, it only happened if I go hog wild pressing buttons and changing modes.
// 10/19/16: extended length of delay when checking if Halt pressed.
// This code uses numerous byte constants, rather than enum, so they will fit in 1-byte RS485 and data structure fields.

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-BTN.  Byte numbers represent offsets, so they start at zero. ***
// Because there are so many message types, we will document the protocols here and just use "magic numbers" for offsets in the code.

// A-MAS BROADCAST: Mode change.  We care about mode because we only look for control panel turnout button presses while in Manual mode, Running state.
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  7
//      1  To         Byte  99 (ALL)
//      2  From       Byte  1 (A_MAS)
//      3  Msg Type   Char  'M' means this is a Mode/State update message
//      4  Mode       Byte  1..5 [Manual | Register | Auto | Park | POV]
//      5  State      Byte  1..3 [Running | Stopping | Stopped]
//      6  Cksum      Byte  0..255

// A-MAS to A-BTN: Permission for A-BTN to send which turnout button was just pressed on the control panel
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  5
//      1  To         Byte  4 (A_BTN)
//      2  From       Byte  1 (A_MAS)
//      3  Msg type   Char  'B' = Button request status (sent after it sees that a button was pressed, via a digital input from A-BTN.)
//      4  Checksum   Byte  0..255

// A-BTN to A-MAS: Sending the number of the turnout button that was just pressed on the control panel
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  6
//      1  To         Byte  1 (A_MAS)
//      2  From       Byte  4 (A_BTN)
//      3  Msg type   Char  'B' = Button press update
//      4  Button No. Byte  1..32
//      5  Checksum   Byte  0..255

// **************************************************************************************************************************

// We will start in MODE_UNDEFINED, STATE_STOPPED.  We must receive a message from A-MAS to tell us otherwise.
byte modeCurrent  = MODE_UNDEFINED;
byte stateCurrent = STATE_STOPPED;
                              
// *** SERIAL LCD DISPLAY CLASS:
// We will create an object called "LCD" that displays messages on the 2004 display (i.e. LCD.send)  Although this object requires that we have
// previously created a "digoleLCD" object from the DigoleSerial class library, "LCD" is not a child class/object of digoleLCD.  When we instantiate
// our LCD object, we'll pass it a pointer to the digoleLCD object, and then we can forget about the Digole class/object altogether.
#define _Digole_Serial_UART_                  // Tell compiler to use only the serial part of the DigoleSerial class library
#include <DigoleSerial.h>                     // Tell the compiler to use the DigoleSerial class library
// DigoleSerialDisp needs serial port address i.e. &Serial, &Serial1, &Serial2, or &Serial3.
// DigoleSerialDisp also needs a valid baud rate, typically 9600 or 115200.
DigoleSerialDisp digoleLCD(&Serial1, SERIAL1_SPEED);  // Instantiate and name the DigoleSerial object, required for the Display_2004 class.
#include <Display_2004.h>                     // Our LCD message-display library; requires DigoleSerialDisp object "digoleLCD"
Display_2004 LCD(&digoleLCD);                 // Finally, instantiate our LCD object "LCD" by passing a pointer to the Digole LCD object.
char lcdString[LCD_WIDTH + 1];                // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** RS485 & Digital Pin MESSAGE CLASS (Inter-Arduino communications):
#include <Message_BTN.h>                      // This module's communitcation class, in its .ini directory
Message_BTN Message(&Serial2, SERIAL2_SPEED, &LCD);  // Instantiate our RS485/digital communications object "Message"
byte msgIncoming[RS485_MAX_LEN];              // Global array for incoming inter-Arduino messages.  No need to init contents.
byte msgOutgoing[RS485_MAX_LEN];              // No need to initialize contents.

// *** SHIFT REGISTER: The following lines are required by the Centipede input/output shift registers.
#include <Wire.h>                 // Needed for Centipede shift register
#include <Centipede.h>
Centipede shiftRegister;          // create Centipede shift register object

// *** MISC CONSTANTS AND GLOBALS: needed by A-BTN:
unsigned long lastReleaseTimeMS      =   0;  // To prevent operator from pressing turnout buttons too quickly
const unsigned long RELEASE_DELAY_MS = 200;  // Force operator to wait this many milliseconds between presses

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  LCD.init();                           // Initialize the 20 x 04 Digole serial LCD display object "LCD."  Needs to be done here, not in LCD constructor.
  Serial.begin(SERIAL0_SPEED);          // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  // Serial2 is for our RS485 message bus, already set up
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // Set all Centipede shift register pins to INPUT for monitoring button presses
  initializePinIO();                    // Initialize all of the I/O pins and turn all LEDs on control panel off
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  Serial.println(lcdString);
  LCD.send(lcdString);

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // See if there is an incoming message.  If so, handle accordingly; otherwise watch for a button press.
  // Note that the ONLY message A-BTN cares about is a "mode broadcast" from A-MAS, and also of course
  // we need a "send button change" request message, but only after we ask A-MAS to ask us.

  if (Message.receive(msgIncoming) == true) {
    // We received a new message that is relevant to this module.
    // This has the side effect to clear out any other incoming RS485 messages that we don't care about.
    // At this point, it could only be a mode - change message, else fatal error.
    if (Message.getType(msgIncoming) == 'M') {  // It's a Mode update from A-MAS!
      modeCurrent = Message.getMode(msgIncoming);
      stateCurrent = Message.getState(msgIncoming);
    } else {  // Our message was not a mode update, so that is a fatal error (should never happen)
      sprintf(lcdString, "Wrong msg type!");
      LCD.send(lcdString);
      Serial.print(lcdString);
      endWithFlashingLED(6);
    }
  }

  // Check to see if a turnout button has been pressed, and handle accordingly.
  // But only do so if we are in mode MANUAL and state RUNNING.  Otherwise, do nothing until mode/state changes.

  if ((modeCurrent == MODE_MANUAL) && (stateCurrent == STATE_RUNNING)) {

    // If we get here, then we know we are in MODE_MANUAL, STATE_RUNNING.  This is the only case where we look for button presses.
    // And, don't even bother checking for button press unless we have waited at least RELEASE_DELAY_MS since the last button release.
    if ((millis() - lastReleaseTimeMS) > RELEASE_DELAY_MS) {

      // Okay, it's been at least RELEASE_DELAY_MS since operator released a turnout control button, so check for another press...
      // Find the FIRST control panel pushbutton that is being pressed.  Only handles one at a time.
      byte buttonPressed = turnoutButtonPressed();   // Returns 0 if no button pressed; otherwise button number that was pressed.
      if (buttonPressed > 0) {   // Operator pressed a pushbutton!

        Message.sendTurnoutButtonPress(buttonPressed);  // Don't even need to send it the message buffer!

        // The operator *just* pressed the button, so wait for bounce and final release before moving on...
        turnoutButtonDebounce(buttonPressed);

        lastReleaseTimeMS = millis();   // Keep track of the last time a button was released

      }  // end of "if button pressed" code block
    }    // end of "if we've waited long enough since button was released...
  }      // end of "if we are in manual mode, running state...

}  // end of main loop()


// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

void initializePinIO() {
  // Rev 10/19/16: Initialize all of the input and output pins
  digitalWrite(PIN_SPEAKER, HIGH);  // Piezo
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_LED, HIGH);      // Built-in LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);
  pinMode(PIN_REQ_TX_A_BTN_OUT, OUTPUT);   // When a button has been pressed, tell A-MAS by pulling this pin LOW
  digitalWrite(PIN_HALT, HIGH);
  pinMode(PIN_HALT, INPUT);                  // HALT pin: monitor if gets pulled LOW it means someone tripped HALT.  Or change to output mode and pull LOW if we want to trip HALT.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
  return;
}

void initializeShiftRegisterPins() {
  // Rev: 10/26/16.  Set all Centipede shift register pins to input for monitoring button presses
  for (int i = 0; i < 8; i++) {         // For each of 4 chips per board / 2 boards = 8 chips @ 16 bits/chip
    shiftRegister.portMode(i, 0b1111111111111111);   // 0 = OUTPUT, 1 = INPUT.  Sets all pins on chip to INPUT
    shiftRegister.portPullup(i, 0b1111111111111111); // 0 = NO PULLUP, 1 = PULLUP.  Sets all pins on chip to PULLUP
  }
  return;
}

byte turnoutButtonPressed() {
  // Scans the Centipede input shift register connected to the control panel turnout buttons, and returns the button number being
  // pressed, if any, or zero if no buttons currently being pressed.
  // Rev 08/28/17.
  byte bp = 0;  // This will be set to button number pressed, if any
  for (int pinNum = 0; pinNum < TOTAL_TURNOUTS; pinNum++) {    // For Centipede input pin = 0..31
    if (shiftRegister.digitalRead(pinNum) == LOW) {            // LOW == 0x0; HIGH == 0x1
      bp = pinNum + 1;  // i.e. pin 0 = button/turnout 1
      chirp();  // operator feedback!
      sprintf(lcdString, "Button %2i pressed.", bp);
      LCD.send(lcdString);
      Serial.println(lcdString);
      break;
    }
  }
  return bp;
}

void turnoutButtonDebounce(const byte tButtonPressed) {
  // After we detect a control panel turnout button press, and presumably after we have acted on it, we need a delay
  // before we can resume checking for subsequent button presses (or even another press of this same button.)
  // We must allow for some bounce when the pushbutton is pressed, and then more bounce when it is released.
  // Rev 09/14/17.

  // Wait long enough to for pushbutton to stop possibly bouncing so our look for "release button" will be valid
  delay(20);  // don't let operator press buttons too quickly
  // Now watch and wait for the operator to release the pushbutton...
  while (shiftRegister.digitalRead(tButtonPressed - 1) == LOW) { }    // LOW == 0x0; HIGH == 0x1
  // We also need some debounce after the operator releases the button
  delay(20);
  return;
}

// ***************************************************************************
// *** HERE ARE FUNCTIONS USED BY VIRTUALLY ALL ARDUINOS *** REV: 09-12-16 ***
// ***************************************************************************

void endWithFlashingLED(int numFlashes) {
  // Rev 10/05/16: Version for Arduinos WITHOUT relays that should be released.


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


      chirp();
      sprintf(lcdString, "%.20s", "HALT pin low!  End.");
      LCD.send(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    } else {
      sprintf(lcdString, "False HALT detected.");
      LCD.send(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}
