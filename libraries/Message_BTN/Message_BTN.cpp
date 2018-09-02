// Rev: 07/17/18
// Message_BTN is a child class of Message_RS485, and handles all RS485 and digital pin messages for test.ino.
// A_BTN both sends and receives RS485 messages, and also needs to send a digital signal to A_MAS for request to send.

#include "Message_BTN.h"

// Call the parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_BTN::Message_BTN(HardwareSerial * t_hdwrSerial, long unsigned int t_baud, Display_2004 * t_LCD2004) : Message_RS485(t_hdwrSerial, t_baud, t_LCD2004) { }

// ***** PUBLIC METHODS *****

byte Message_BTN::getMode(const byte t_msg[]) {
  return t_msg[RS485_ALL_MAS_MODE_OFFSET];
}

byte Message_BTN::getState(const byte t_msg[]) {
  return t_msg[RS485_ALL_MAS_STATE_OFFSET];
}

void Message_BTN::sendTurnoutButtonPress(const byte t_button) {  // Rev. 07/12/18
  // button will be in the range 1..n (not starting with zero.)
  // In order to send a button press RS485 message to A_MAS, we must first ask permission via a digital signal wire.
  // Pull the line low, and wait for an incoming button-number request from A_MAS, then set the digital line back
  // to high, and send the button number via an RS485 message.

  // First pull a digital line low, to tell A-MAS that we want to send it a "turnout button pressed" message...

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "BTN pull RTX LOW");
m_myLCD->send(lcdString);

  digitalWrite(PIN_REQ_TX_A_BTN_OUT, LOW);


  // Now just wait for an RS485 command from A-MAS requesting button status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-BTN.
  // m_msgIncoming is garbage, so put something in the "to" field *other than* A_BTN, so when we check to see if there is
  // a message *to* A_BTN, we'll know it wasn't random garbage but a real message.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
//  setTo(m_msgIncoming, ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  byte message[RS485_MAX_LEN];
  do {
    checkIfHaltPinPulledLow();     // If someone has pulled the Halt pin low, just stop
  } while (receive(message) == false);

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "BTN got RS485...");
m_myLCD->send(lcdString);
//delay(1000);

  // It's *possible* that we could receive a mode_change message before A_MAS has a chance to check for a turnout button request.
  // So we conceivable could miss a mode-change message here.  But only if the operator changed modes while basically pressing a
  // turnout button at the same time.  I'll call that operator error.
  // Now we have an RS485 message sent to A_BTN.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  // If not coming from A_MAS or not an 'B'-type (button press) request, something is wrong.
  if ((getFrom(message) != ARDUINO_MAS) || (getType(message) != 'B')) {
    sprintf(lcdString, "ERR to = %i3", getFrom(message));
    m_myLCD->send(lcdString);
    Serial.print(lcdString);
    sprintf(lcdString, "ERR type = %i3", getType(message));
    m_myLCD->send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }
  // Release the digital pin "I have a message for you, A-MAS" back to HIGH state...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);
  // Format and send the new status: Length, To, From, 'B', button number (byte).  CRC is automatic in class.
  setLen(message, 6);
  setTo(message, ARDUINO_MAS);
  setFrom(message, ARDUINO_BTN);
  setType(message, 'B');
  setTurnoutButtonNum(message, t_button);  // Button number 1..32 (not 0..31)

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "BTN sending RS485");
m_myLCD->send(lcdString);

  send(message);

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "BTN sent RS485");
m_myLCD->send(lcdString);
//delay(1000);

  return;
}

// ***** PRIVATE METHODS *****

void Message_BTN::send(byte t_msg[]) {     // Private method Rev. 07/12/18
  Message_RS485::RS485SendMessage(t_msg);
  return;
}

bool Message_BTN::receive(byte t_msg[]) {  // Private method Rev. 07/18/18
  // Try to retrieve a complete incoming message.  
  // NON-DESTRUCTIVE TO BUFFER: If we receive a real message, but the calling module doesn't want it, then we will *not* overwrite t_msg[]
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true (and populates t_msg[]) if there was a complete message *and* it's one that we want to see.
  // Thus, uses Message_RS485::m_msgIncoming as a temporary buffer for the message...
  // First see if there is a valid message of any type...
  byte message[RS485_MAX_LEN];

  if ((Message_RS485::RS485GetMessage(message)) == false) return false;
  // Okay, there was a real message found and it's stored in m_msgIncoming.
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_BTN only cares about two incoming messages:
  //   A_MAS to ALL Mode broadcase
  //   A_MAS to A_BTN "send the button number that was just pressed."
  if ((getTo(message) == ARDUINO_ALL) && (getFrom(message) == ARDUINO_MAS) && (getType(message) == 'M')) {
    memcpy(t_msg, message, RS485_MAX_LEN);  // Copy the contents of the new message buffer into the "return" t_msg[] buffer
    return true;  // It's a Mode change broadcast message
  }
  else if ((getTo(message) == ARDUINO_BTN) && (getFrom(message) == ARDUINO_MAS) && (getType(message) == 'B')) {
    memcpy(t_msg, message, RS485_MAX_LEN);  // Copy the contents of the new message buffer into the "return" t_msg[] buffer
    return true;  // It's a request from A_MAS to send the button number of the control panel button just pressed
  }
  else {  // Not a message we are interested in
sprintf(lcdString, "%.20s", "RS485 we don't want!");
m_myLCD->send(lcdString);
sprintf(lcdString, "getTo = %3i", (int)getTo(message));  // Master is seeing "To Arduino #4" which means incoming message is to A_BTN!  Only outgoing should be to A_BTN.*****************************
m_myLCD->send(lcdString);
sprintf(lcdString, "getFrom = %3i", (int)getFrom(message));  // Master is seeing "From Arduino #1" which means incoming message is from A_MAS!  Only outgoing should be from A_MAS. ****************
m_myLCD->send(lcdString);
sprintf(lcdString, "getType = %3c", getType(message));
m_myLCD->send(lcdString);
while (1) {}   // ********* DEBUG **************************************************************************************************************************************
    return false;
  }
}

void Message_BTN::setTurnoutButtonNum(byte t_msg[], byte t_button) {  // Private method inserts button number into the appropriate byte
  t_msg[RS485_MAS_BTN_BUTTON_NUM_OFFSET] = t_button;
  return;
}

