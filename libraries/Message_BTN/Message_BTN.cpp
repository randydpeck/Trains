// Rev: 07/14/18
// Message_BTN is a child class of Message_RS485, and handles all RS485 and digital pin messages for test.ino.

#include <Message_BTN.h>

// xxxxxxxxxx  Because A_SWT only receives messages and does not send any, this class does not include any "send" logic.
// xxxxxxxxxxxx Because A_SWT does not send or receive any digital messages (i.e. Arduino pins), this class does not include that logic either.
// A_BTN both sends and receives RS485 messages, and also needs to send a digital signal.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_BTN::Message_BTN(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) : Message_RS485(hdwrSerial, baud, LCD2004) { }

// ***** PUBLIC METHODS *****

bool Message_BTN::receive(byte tMsg[]) {  // Private method Rev. 07/12/18

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.
  // If we do receive a real message, but the calling module doesn't want it, then do *not* overwrite tMsg[]
  // Thus, create a temporary buffer for the message...
  byte msgBuffer[RS485_MAX_LEN];

  // First see if there is a valid message of any type...
  if ((Message_RS485::RS485GetMessage(msgBuffer)) == false) return false;

  // Okay, there was a real message found and it's stored in msgBuffer.
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_BTN only cares about two incoming messages:
  //   A_MAS to ALL Mode broadcase
  //   A_MAS to A_BTN "send the button number that was just pressed."

  if ((getTo(msgBuffer) == ARDUINO_ALL) && (getFrom(msgBuffer) == ARDUINO_MAS) && (getType(msgBuffer) == 'M')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);  // Copy the contents of the new message buffer into the "return" tMsg[] buffer
    return true;  // It's a Mode change broadcast message
  }
  else if ((getTo(msgBuffer) == ARDUINO_BTN) && (getFrom(msgBuffer) == ARDUINO_MAS) && (getType(msgBuffer) == 'B')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);  // Copy the contents of the new message buffer into the "return" tMsg[] buffer
    return true;  // It's a request from A_MAS to send the button number of the control panel button just pressed
  }
  else {  // Not a message we are interested in
    return false;
  }
}

byte Message_BTN::getMode(byte tMsg[]) {
  return tMsg[RS485_ALL_MAS_MODE_OFFSET];
}

byte Message_BTN::getState(byte tMsg[]) {
  return tMsg[RS485_ALL_MAS_STATE_OFFSET];
}

void Message_BTN::sendTurnoutButtonPress(const byte button) {  // Rev. 07/12/18

  // In order to send a button press RS485 message to A_MAS, we must first ask permission via a digital signal wire.
  // Pull the line low, and wait for an incoming button-number request from A_MAS, then set the digital line back
  // to high, and send the button number via an RS485 message.

  byte msgBuffer[RS485_MAX_LEN];  // Since we aren't passed msgIncoming or msgOutgoing, create a temp outgoing message buffer.

  // First pull a digital line low, to tell A-MAS that we want to send it a "turnout button pressed" message...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, LOW);

  // Now just wait for an RS485 command from A-MAS requesting button status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-BTN.
  // msgBuffer is garbage, so put something in the "to" field *other than* A_BTN, so when we check to see if there is
  // a message *to* A_BTN, we'll know it wasn't random garbage but a real message.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
//  this->setTo(msgBuffer, ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  setTo(msgBuffer, ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  do {
    checkIfHaltPinPulledLow();     // If someone has pulled the Halt pin low, just stop
  } while (receive(msgBuffer) == false);
//    this->receive(msgBuffer);      // Will only populate msgBuffer if a new message came in since last call
//    receive(msgBuffer);      // Will only populate msgBuffer if a new message came in (and that we care about!) since last call
//  } while ((getTo(msgBuffer)) == ARDUINO_NUL);
//} while ((this->getTo(msgBuffer)) == ARDUINO_NUL);
//} while ((this->getTo(msgBuffer)) != ARDUINO_BTN);

  sprintf(lcdString, "%.20s", "Got request!");
  myLCD->send(lcdString);

  // It's *possible* that we could receive a mode_change message before A_MAS has a chance to check for a turnout button request.
  // So we conceivable could miss a mode-change message here.  But only if the operator changed modes while basically pressing a
  // turnout button at the same time.  I'll call that operator error.

  // Now we have an RS485 message sent to A_BTN.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  // If not coming from A_MAS or not an 'B'-type (button press) request, something is wrong.
  // if ((this->getFrom(msgBuffer) != ARDUINO_MAS) || (this->getType(msgBuffer) != 'B')) {
  if ((getFrom(msgBuffer) != ARDUINO_MAS) || (getType(msgBuffer) != 'B')) {
    sprintf(lcdString, "%.20s", "Unexpected message!");
    myLCD->send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }

  // Release the digital pin "I have a message for you, A-MAS" back to HIGH state...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);

  // Format and send the new status: Length, To, From, 'B', button number (byte).  CRC is automatic in class.
  //this->setLen(msgBuffer, 6);
  //this->setTo(msgBuffer, ARDUINO_MAS);
  //this->setFrom(msgBuffer, ARDUINO_BTN);
  //this->setType(msgBuffer, 'B');
  //this->setTurnoutButtonNum(msgBuffer, button);  // Button number 1..32 (not 0..31)
  //this->send(msgBuffer);

  setLen(msgBuffer, 6);
  setTo(msgBuffer, ARDUINO_MAS);
  setFrom(msgBuffer, ARDUINO_BTN);
  setType(msgBuffer, 'B');
  setTurnoutButtonNum(msgBuffer, button);  // Button number 1..32 (not 0..31)
  send(msgBuffer);

  //sprintf(lcdString, "%.20s", "Sent btn num!");
  //myLCD->send(lcdString);

  return;
}

// ***** PRIVATE METHODS *****

void Message_BTN::send(byte tMsg[]) {  // Private method to this class.  Sends of specific type are public.

  Message_RS485::RS485SendMessage(tMsg);
  return;
 
}

void Message_BTN::setTurnoutButtonNum(byte tMsg[], byte button) {  // Private method inserts button number into the appropriate byte

  tMsg[RS485_MAS_BTN_BUTTON_NUM_OFFSET] = button;
  return;

}

