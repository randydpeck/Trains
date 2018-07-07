// Rev: 07/07/18
// Message_BTN is a child class of Message_RS485, and handles all RS485 and digital pin messages for test.ino.

#include <Message_BTN.h>

// xxxxxxxxxx  Because A_SWT only receives messages and does not send any, this class does not include any "send" logic.
// xxxxxxxxxxxx Because A_SWT does not send or receive any digital messages (i.e. Arduino pins), this class does not include that logic either.
// A_BTN both sends and receives RS485 messages, and also needs to send a digital signal.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_BTN::Message_BTN(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) : Message_RS485(hdwrSerial, baud, LCD2004) {

  mySerial = hdwrSerial;  // Pointer to the serial port we want to use for RS485 (probably not needed here)
  myBaud = baud;          // Serial port baud rate (probably not needed here)
  myLCD = LCD2004;        // Pointer to the LCD display for error messages (possibly not needed here)
  
}

void Message_BTN::Init() {

  Message_RS485::InitPort();
  return;

}

void Message_BTN::Send(byte tMsg[]) {

  Message_RS485::RS485SendMessage(tMsg);
  return;
 
}

bool Message_BTN::Receive(byte tMsg[]) {

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.

  // First see if there is a valid message of any type...
  if ((Message_RS485::RS485GetMessage(tMsg)) == false) return false;
  return true;
  // Okay, there was a real message found and it's stored in tMsg[].
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_BTN only cares about two incoming messages:
  //   A_LEG to ALL Mode broadcase
  //   A_LEG to A_BTN "send the button number that was just pressed."

  if ((tMsg[RS485_TO_OFFSET] == ARDUINO_ALL) && (tMsg[RS485_FROM_OFFSET] == ARDUINO_MAS) && (tMsg[RS485_TYPE_OFFSET] == 'M')) {
    return true;  // It's a Mode change broadcast message
  } else   if ((tMsg[RS485_TO_OFFSET] == ARDUINO_ALL) && (tMsg[RS485_FROM_OFFSET] == ARDUINO_MAS) && (tMsg[RS485_TYPE_OFFSET] == 'B')) {
    return true;  // It's a request from A_MAS to send the button number of the control panel button just pressed
  } else {  // Not a message we are interested in
    return false;
  }
}

void Message_BTN::SendTurnoutButtonPress(const byte button) {

  // In order to send a button press RS485 message to A_MAS, we must first ask permission via a digital signal wire.
  // Pull the line low, and wait for an incoming button-number request from A_MAS, then set the digital line back
  // to high, and send the button number via an RS485 message.

  byte MsgBuffer[RS485_MAX_LEN];  // Since we aren't passed MsgIncoming or MsgOutgoing, create a temp outgoing message buffer.

  // First pull a digital line low, to tell A-MAS that we want to send it a "turnout button pressed" message...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, LOW);

  // Now just wait for an RS485 command from A-MAS requesting button status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-BTN.
  // MsgBuffer is garbage, so put something in the "to" field *other than* A_BTN, so when we check to see if there is
  // a message *to* A_BTN, we'll know it wasn't random garbage but a real message.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
  this->SetTo(MsgBuffer, ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  do {
    checkIfHaltPinPulledLow();     // If someone has pulled the Halt pin low, just stop
    this->Receive(MsgBuffer);      // Will only populate MsgBuffer if a new message came in since last call
  } while ((this->GetTo(MsgBuffer)) != ARDUINO_BTN);

  //sprintf(lcdString, "Got request!");
  //myLCD->send(lcdString);

  // It's *possible* that we could receive a mode_change message before A_MAS has a chance to check for a turnout button request.
  // So we conceivable could mess a mode-change message here.  But only if the operator changed modes while basically pressing a
  // turnout button at the same time.  I'll call that operator error.

  // Now we have an RS485 message sent to A_BTN.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  // If not coming from A_MAS or not an 'B'-type (button press) request, something is wrong.
  if ((this->GetFrom(MsgBuffer) != ARDUINO_MAS) || (this->GetType(MsgBuffer) != 'B')) {
    sprintf(lcdString, "Unexpected message!");
    myLCD->send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }

  // Release the digital pin "I have a message for you, A-MAS" back to HIGH state...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);

  // Format and send the new status: Length, To, From, 'B', button number (byte).  CRC is automatic in class.
  this->SetLen(MsgBuffer, 6);
  this->SetTo (MsgBuffer, ARDUINO_MAS);
  this->SetFrom(MsgBuffer, ARDUINO_BTN);
  this->SetType(MsgBuffer, 'B');
  this->SetTurnoutButtonNum(MsgBuffer, button);  // Button number 1..32 (not 0..31)
  this->Send(MsgBuffer);

  //sprintf(lcdString, "Sent btn num!");
  //myLCD->send(lcdString);
  
  return;
}

void Message_BTN::SetTurnoutButtonNum(byte tMsg[], byte button) {  // Inserts button number into the appropriate byte
  tMsg[RS485_BTN_BUTTON_NO_OFFSET] = button;
  return;
}

byte Message_BTN::GetTurnoutButtonPress() {

  // This is only called (periodically) when in MANUAL and maybe P.O.V. modes.
  // Checks to see if operator pressed a "throw turnout" button on the control panel, and executes if so.
  // Check status of pin 8 (coming from A-BTN) and see if it's been pulled LOW, indicating button was pressed.
  if ((digitalRead(PIN_REQ_TX_A_BTN_IN)) == HIGH) {

    //sprintf(lcdString, "Line is high.");
    //myLCD->send(lcdString);

    return 0;  // A_BTN is not asking to send us a button press message
  
  } else {     // A_BTN is asking to send us a button press message!

    //sprintf(lcdString, "Line went low!");
    //myLCD->send(lcdString);

    byte MsgBuffer[RS485_MAX_LEN];  // Since we aren't passed MsgIncoming or MsgOutgoing, create a temp outgoing message buffer.

    this->SetLen(MsgBuffer, 5);
    this->SetTo(MsgBuffer, ARDUINO_BTN);
    this->SetFrom(MsgBuffer, ARDUINO_MAS);
    this->SetType(MsgBuffer, 'B');  // Button number request
    this->Send(MsgBuffer);

    //sprintf(lcdString, "Sent request to BTN");
    //myLCD->send(lcdString);

    // Now wait for a reply...
    // There is NO legitimate reason why we would get ANY RS485 message from anyone except A-BTN at this point.
    MsgBuffer[RS485_TO_OFFSET] = 0;   // Set "to" field to anything other than ARDUINO_MAS
    do {
      this->Receive(MsgBuffer);  // Only returns with data if it has a complete new message
    } while (MsgBuffer[RS485_TO_OFFSET] != ARDUINO_MAS);
    // We should NEVER get a message that isn't to A-MAS from A-BTN, but we'll check anyway...
    if ((this->GetTo(MsgBuffer) !=  ARDUINO_MAS) || (this->GetFrom(MsgBuffer) != ARDUINO_BTN)) {
      sprintf(lcdString, "%.20s", "RS485 to/from error!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    
    byte buttonNum = this->GetTurnoutButtonNum(MsgBuffer);   // Button number 1..32
    //sprintf(lcdString, "Got btn %2i.", buttonNum);
    //myLCD->send(lcdString);

    if ((buttonNum < 1) || (buttonNum > TOTAL_TURNOUTS)) {
      sprintf(lcdString, "%.20s", "RS485 bad button no!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // Okay, the operator pressed a control panel pushbutton to toggle a turnout, and we have the turnout number 1..32.
    return buttonNum;
  }
}

byte Message_BTN::GetTurnoutButtonNum(byte tMsg[]) {
  return tMsg[RS485_BTN_BUTTON_NO_OFFSET];
}
