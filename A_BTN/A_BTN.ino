#include <Train_Consts_Global.h>
char APPVERSION[LCD_WIDTH + 1] = "A-BTN Rev. 10/01/17";
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?

// A_BTN watches for control panel pushbutton presses by operator, and transmits that info to A_MAS.
// This is an "OUTPUT-ONLY" module that does not monitor or respond to any other Arduino (it does watch for A-MAS mode changes.)
// A_BTN is only active when the system is in Manual mode, Running state.  Turnout button presses are ignored during all other
// modes and states.  P.O.V. support can be added later if relevant.

// 04/22/18: Updating new global const, message and LCD display classes.
// 09/16/17: Brought a lot of loop code out to functions; much streamlining, variable re-naming.
// 09/13/17: Changed variable suffixes and constant names for better programming form.
// 08/28/17: Const LCD width, converted some inline loop code to functions, added RS485 protocol comments, and other clean-up.
// 11/01/16: InitializeShiftRegisterPins() and delay in LCD2004.send()
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
// OFFSET DESC      SIZE  CONTENTS
//   	0	  Length	  Byte	7
//   	1	  To	      Byte	99 (ALL)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'M' means this is a Mode/State update command
//   	4	  Mode	    Byte  1..5 [Manual | Register | Auto | Park | POV]
//   	5	  State	    Byte  1..3 [Running | Stopping | Stopped]
//   	6	  Cksum	    Byte  0..255

// A-MAS to A-BTN: Permission for A-BTN to send which turnout button was just pressed on the control panel
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//    0	  Length	  Byte	5
//    1	  To	      Byte	4 (A_BTN)
//    2	  From	    Byte	1 (A_MAS)
//    3	  Msg type	Char	'B' = Button request status (sent after it sees that a button was pressed, via a digital input from A-BTN.)
//    4	  Checksum	Byte  0..255

// A-BTN to A-MAS: Sending the number of the turnout button that was just pressed on the control panel
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//    0	  Length	  Byte	6
//    1	  To	      Byte	1 (A_MAS)
//    2	  From	    Byte	4 (A_BTN)
//    3	  Msg type	Char	'B' = Button press update
//    4	  Button No.Byte  1..32
//    5	  Checksum	Byte  0..255

// **************************************************************************************************************************

// We will start in MODE_UNDEFINED, STATE_STOPPED.  We must receive a message from A-MAS to tell us otherwise.
byte modeCurrent  = MODE_UNDEFINED;
byte stateCurrent = STATE_STOPPED;
                              
// *** SERIAL LCD DISPLAY CLASS:
#include <Display_2004.h>                   // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
// Instantiate a Display_2004 object called "LCD2004".
// Pass address of serial port to use (0..3) such as &Serial1, and baud rate such as 9600 or 115200.
Display_2004 LCD2004(&Serial1, 9600);     // Instantiate 2004 LCD display "LCD2004."
Display_2004 * ptrLCD2004;                  // Pointer will be passed to any other classes that need to be able to write to the LCD display.
char lcdString[LCD_WIDTH + 1];              // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** MESSAGE CLASS (Inter-Arduino communications):
#include <Message_BTN.h>
Message_BTN Message(&Serial2, ptrLCD2004);  // Instantiate message object "Message"; requires a pointer to the hardware serial port, and the 2004 LCD display
byte MsgIncoming[RS485_MAX_LEN];            // Global array for incoming inter-Arduino messages.  No need to init contents.  Probably shouldn't call them "RS485" though.
byte MsgOutgoing[RS485_MAX_LEN];            // No need to initialize contents.

// *** SHIFT REGISTER: The following lines are required by the Centipede input/output shift registers.
#include <Wire.h>                 // Needed for Centipede shift register
#include <Centipede.h>
Centipede shiftRegister;          // create Centipede shift register object

// *** MISC CONSTANTS AND GLOBALS: needed by A-BTN:
const byte TOTAL_TURNOUTS            =  32;  // How many input bits on Centipede shift register to check.  30 connected, but 32 relays.
unsigned long lastReleaseTimeMS      =   0;  // To prevent operator from pressing turnout buttons too quickly
const unsigned long RELEASE_DELAY_MS = 200;  // Force operator to wait this many milliseconds between presses

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  LCD2004.init();                       // Initialize the 20 x 04 Digole serial LCD display
  delay(1000);
  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485  up to 115200 ********** THIS SHOULD BE MOVED INTO Message_RS485.h/.cpp and commented out here
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // Set all Centipede shift register pins to INPUT for monitoring button presses
  initializePinIO();                    // Initialize all of the I/O pins and turn all LEDs on control panel off
  
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  Serial.println(lcdString);
  LCD2004.send(lcdString);


  Message.Hello();

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // Look for RS485 message, and if found, see if it's a mode-update message.  Otherwise ignore.
  // Note that the ONLY message A-BTN cares about is a "mode broadcast" from A-MAS, and also of course
  // we need a "send button change" request message, but only after we ask A-MAS to ask us.
  // Also take this opportunity to clear out any other incoming RS485 messages that we don't care about...

  // We will just sit in a loop watching RS485 incoming messages until/unless the mode is MODE_MANUAL, STATE_RUNNING
  // We can only escape the following do..while loop if mode is Manual and state is Running.

  do {

    getModeAndState(&modeCurrent, &stateCurrent);  // Updates the two parameters if anything has changed since last check
    checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  } while ((modeCurrent != MODE_MANUAL) || (stateCurrent != STATE_RUNNING));

  // If we get here, then we know we are in MODE_MANUAL mode, STATE_RUNNING state.  This is the only case where we look for button presses.
  // And, don't even bother checking for button press unless we have waited at least RELEASE_DELAY_MS since the last button release.

  if ((millis() - lastReleaseTimeMS) > RELEASE_DELAY_MS) {

    // Okay, it's been at least RELEASE_DELAY_MS since operator released a turnout control button, so check for another press...
    // Find the FIRST control panel pushbutton that is being pressed.  Only handles one at a time.

    byte buttonPressed = turnoutButtonPressed();   // Returns 0 if no button pressed; otherwise button number that was pressed.

    if (buttonPressed > 0) {   // Operator pressed a pushbutton!

      // Send an RS485 message to A-MAS indicating which turnout pushbutton was pressed.  1..32 (not 0..31)
      sendRS485ButtonPressUpdate(buttonPressed);

      // The operator *just* pressed the button, so wait for bounce and final release before moving on...
      turnoutButtonDebounce(buttonPressed);

      lastReleaseTimeMS = millis();   // Keep track of the last time a button was released

    }  // end of "if button pressed" code block

  }   // end of "if we've waited long enough since button was released...

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

void getModeAndState(byte * cm, byte * cs) {
  // Changes the value of modeCurrent and/or stateCurrent if A-MAS sends us a mode-change message; otherwise leave as-is.
  if (Message.Receive(MsgIncoming)) {  // If returns true, then we got a complete RS485 message (may or may not be for us)
    if ((Message.GetTo(MsgIncoming)) == ARDUINO_ALL) {
      if ((Message.GetFrom(MsgIncoming)) == ARDUINO_MAS) {
        if (Message.GetType(MsgIncoming) == 'M') {  // It's a Mode update from A-MAS!
          *cm = MsgIncoming[4];
          *cs = MsgIncoming[5];
        }
      }
    }
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
      LCD2004.send(lcdString);
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

void sendRS485ButtonPressUpdate(const byte tButtonPressed) {
  // Send an RS485 message to A-MAS indicating which turnout toggle button the operator just pressed.
  // Rev 09/13/17.

  // First pull PIN_REQ_TX_A_BTN_OUT digital line low, to tell A-MAS that we want to send it a "turnout button pressed" message...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, LOW);

  // Wait for an RS485 command from A-MAS requesting button status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-BTN.
  // If there is no new message, then MsgIncoming will remain unchanged - whatever it was before.  Since the previous
  // message might have been to us, we'd better overwrite that attribute so we won't think it's a new message.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
  Message.SetTo(MsgIncoming, ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  do {
    checkIfHaltPinPulledLow();     // If someone has pulled the Halt pin low, just stop
    Message.Receive(MsgIncoming);  // Will only populate MsgIncoming with new data if a new message came in since last call
  } while ((Message.GetTo(MsgIncoming)) != ARDUINO_BTN);

  // Now we have an RS485 message sent to A_BTN.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  // If not coming from A_MAS or not an 'B'-type (button press) request, something is wrong.
  if ((Message.GetFrom(MsgIncoming) != ARDUINO_MAS) || (Message.GetType(MsgIncoming) != 'B')) {
    sprintf(lcdString, "Unexpected message!");
    LCD2004.send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }

  // Release the REQ_TX_A_BTN "I have a message for you, A-MAS" digital line back to HIGH state...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);

  // Format and send the new status: Length, To, From, 'B', button number (byte).  CRC is automatic in class.
  Message.SetLen(MsgOutgoing, 6);
  Message.SetTo (MsgOutgoing, ARDUINO_MAS);
  Message.SetFrom(MsgOutgoing, ARDUINO_BTN);
  Message.SetType(MsgOutgoing, 'B');
  Message.SetButtonNo(MsgOutgoing, tButtonPressed);  // Button number 1..32 (not 0..31)
  Message.Send(MsgOutgoing);

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
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    } else {
      sprintf(lcdString, "False HALT detected.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}
