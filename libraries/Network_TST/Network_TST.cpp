// Rev: 05/06/18
// Network_TST is a child class of Message_RS485, and handles all RS485 and digital pin messages for test.ino.

#include "Network_TST.h"

// xxxxxxxxxx  Because A_SWT only receives messages and does not send any, this class does not include any "send" logic.
// xxxxxxxxxxxx Because A_SWT does not send or receive any digital messages (i.e. Arduino pins), this class does not include that logic either.
// A_BTN both sends and receives RS485 messages, and also needs to send a digital signal.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Network_TST::Network_TST(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) : Network_RS485(hdwrSerial, baud, LCD2004) {

  m_Serial = hdwrSerial;  // Pointer to the serial port we want to use for RS485 (probably not needed here)
  m_Baud = baud;          // Serial port baud rate (probably not needed here)
  m_LCD = LCD2004;        // Pointer to the LCD display for error messages (possibly not needed here)
  
}

void Network_TST::Init() {

  Network_RS485::InitPort();
  return;

}

void Network_TST::Send() {

  Network_RS485::RS485SendMessage();
  return;
 
}

bool Network_TST::Receive() {

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.


  ClearIncomingMessage();



  // First see if there is a valid message of any type...
  if ((Network_RS485::RS485GetMessage()) == false) return false;
  
  sprintf(lcdString, "Got RS485 msg!");
  m_LCD->send(lcdString);

  // Okay, there was a real message found and it's stored in tMsg[].
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_BTN only cares about two incoming messages:
  //   A_LEG to ALL Mode broadcase
  //   A_LEG to A_BTN "send the button number that was just pressed."

  if ((GetTo() == ARDUINO_ALL) && (GetFrom() == ARDUINO_MAS) && (GetType() == 'M')) {
    return true;  // It's a Mode change broadcast message
  } else   if ((GetTo() == ARDUINO_ALL) && (GetFrom() == ARDUINO_MAS) && (GetType() == 'B')) {
    return true;  // It's a request from A_MAS to send the button number of the control panel button just pressed
  } else   if ((GetTo() == ARDUINO_BTN) && (GetFrom() == ARDUINO_MAS) && (GetType() == 'B')) {
    return true;
  } else {  // Not a message we are interested in
    return false;
  }
}

void Network_TST::SendTurnoutButtonPress(const byte button) {

  // In order to send a button press RS485 message to A_MAS, we must first ask permission via a digital signal wire.
  // Pull the line low, and wait for an incoming button-number request from A_MAS, then set the digital line back
  // to high, and send the button number via an RS485 message.

//  byte MsgBuffer[RS485_MAX_LEN];  // Since we aren't passed MsgIncoming or MsgOutgoing, create a temp outgoing message buffer.

  // First pull a digital line low, to tell A-MAS that we want to send it a "turnout button pressed" message...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, LOW);

  // Now just wait for an RS485 command from A-MAS requesting button status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-BTN.
  // MsgBuffer is garbage, so put something in the "to" field *other than* A_BTN, so when we check to see if there is
  // a message *to* A_BTN, we'll know it wasn't random garbage but a real message.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
  SetTo(ARDUINO_NUL);   // Anything other than ARDUINO_BTN
  do {
    checkIfHaltPinPulledLow();     // If someone has pulled the Halt pin low, just stop
    Receive();      // Will only populate MsgBufIn if a new message came in since last call
    if (GetTo() != ARDUINO_NUL) {
      sprintf(lcdString, "Got non NULL!");
      m_LCD->send(lcdString);
    
    }
  } while ((GetTo()) != ARDUINO_BTN);

  sprintf(lcdString, "Got request!");
  m_LCD->send(lcdString);

  // It's *possible* that we could receive a mode_change message before A_MAS has a chance to check for a turnout button request.
  // So we conceivable could mess a mode-change message here.  But only if the operator changed modes while basically pressing a
  // turnout button at the same time.  I'll call that operator error.

  // Now we have an RS485 message sent to A_BTN.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  // If not coming from A_MAS or not an 'B'-type (button press) request, something is wrong.
  if ((GetFrom() != ARDUINO_MAS) || (GetType() != 'B')) {
    sprintf(lcdString, "Unexpected message!");
    m_LCD->send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }

  // Release the digital pin "I have a message for you, A-MAS" back to HIGH state...
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);

  sprintf(lcdString, "Sending btn num!");
  m_LCD->send(lcdString);

  // Format and send the new status: Length, To, From, 'B', button number (byte).  CRC is automatic in class.
  SetLen(6);
  SetTo (ARDUINO_MAS);
  SetFrom(ARDUINO_BTN);
  SetType('B');
  SetTurnoutButtonNum(button);  // Button number 1..32 (not 0..31)
  Send();

  sprintf(lcdString, "Sent btn num!");
  m_LCD->send(lcdString);
  
  return;
}

void Network_TST::SetTurnoutButtonNum(byte button) {  // Inserts button number into the appropriate byte
  MsgBufOut[RS485_BTN_BUTTON_NO_OFFSET] = button;
  return;
}

byte Network_TST::GetTurnoutButtonPress() {

  // This is only called (periodically) when in MANUAL and maybe P.O.V. modes.
  // Checks to see if operator pressed a "throw turnout" button on the control panel, and executes if so.
  // Check status of pin 8 (coming from A-BTN) and see if it's been pulled LOW, indicating button was pressed.
  if ((digitalRead(PIN_REQ_TX_A_BTN_IN)) == HIGH) {

    sprintf(lcdString, "Line is high.");
    m_LCD->send(lcdString);

    return 0;  // A_BTN is not asking to send us a button press message
  
  } else {     // A_BTN is asking to send us a button press message!

    sprintf(lcdString, "Line went low!");
    m_LCD->send(lcdString);

    SetLen(5);
    SetTo(ARDUINO_BTN);
    SetFrom(ARDUINO_MAS);
    SetType('B');  // Button number request
    Send();

    sprintf(lcdString, "Sent request to BTN");
    m_LCD->send(lcdString);

    // Now wait for a reply...
    // There is NO legitimate reason why we would get ANY RS485 message from anyone except A-BTN at this point.
    do {
      Receive();  // Key fields set to null if nothing received
    } while (GetTo() != ARDUINO_MAS);
    // We should NEVER get a message that isn't to A-MAS from A-BTN, but we'll check anyway...
    if ((GetTo() !=  ARDUINO_MAS) || (GetFrom() != ARDUINO_BTN)) {
      sprintf(lcdString, "%.20s", "RS485 to/from error!");
      m_LCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    
    byte buttonNum = GetTurnoutButtonNum();   // Button number 1..32
    sprintf(lcdString, "Got btn %2i.", buttonNum);
    m_LCD->send(lcdString);

    if ((buttonNum < 1) || (buttonNum > TOTAL_TURNOUTS)) {
      sprintf(lcdString, "%.20s", "RS485 bad button no!");
      m_LCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // Okay, the operator pressed a control panel pushbutton to toggle a turnout, and we have the turnout number 1..32.
    return buttonNum;
  }
}

byte Network_TST::GetTurnoutButtonNum() {
  return MsgBufIn[RS485_BTN_BUTTON_NO_OFFSET];
}
