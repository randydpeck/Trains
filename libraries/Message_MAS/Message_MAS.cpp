// Rev: 07/14/18
// Message_MAS is a child class of Message_RS485, and handles all RS485 and digital pin messages for A_MAS.ino.

#include <Message_MAS.h>

// A_MAS both sends and receives RS485 messages, and also needs to read digital pin signals from other modules.



// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_MAS::Message_MAS(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) : Message_RS485(hdwrSerial, baud, LCD2004) { }

// ***** PUBLIC METHODS *****

bool Message_MAS::receive(byte tMsg[]) {  // Public method Rev. 07/12/18

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
  // NOTE: Since this is A-MAS, it's going to care about any message type coming in, since it's the master ;-)
  // Specifically, Sensor change updates from A-SNS, Button presses from A-BTN, and Registration and Question data from A-OCC.

  if ((msgBuffer[RS485_TO_OFFSET] == ARDUINO_MAS) && (msgBuffer[RS485_FROM_OFFSET] == ARDUINO_SNS) && (msgBuffer[RS485_TYPE_OFFSET] == 'S')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);
    return true;  // It's a Sensor change response from A-SNS
  } else   if ((msgBuffer[RS485_TO_OFFSET] == ARDUINO_MAS) && (msgBuffer[RS485_FROM_OFFSET] == ARDUINO_BTN) && (msgBuffer[RS485_TYPE_OFFSET] == 'B')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);
    return true;  // It's a Button press response from A-BTN
  } else   if ((msgBuffer[RS485_TO_OFFSET] == ARDUINO_MAS) && (msgBuffer[RS485_FROM_OFFSET] == ARDUINO_OCC) && (msgBuffer[RS485_TYPE_OFFSET] == 'R')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);
    return true;  // It's a Registration response from A-OCC
  } else   if ((msgBuffer[RS485_TO_OFFSET] == ARDUINO_MAS) && (msgBuffer[RS485_FROM_OFFSET] == ARDUINO_OCC) && (msgBuffer[RS485_TYPE_OFFSET] == 'Q')) {
    memcpy(tMsg, msgBuffer, RS485_MAX_LEN);
    return true;  // It's a Question response from A-OCC
  } else {  // Yikes!  Fatal error!
    sprintf(lcdString, "%.20s", "Unexpected message!");
    myLCD->send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }
  return false;  // Code should never get here
}

void Message_MAS::setMode(byte tMsg[], byte tMode) {
  tMsg[RS485_ALL_MAS_MODE_OFFSET] = tMode;
  return;
}

void Message_MAS::setState(byte tMsg[], byte tState) {
  tMsg[RS485_ALL_MAS_STATE_OFFSET] = tState;
  return;
}


byte Message_MAS::getTurnoutButtonPress() {  // Public method Rev. 07/12/18

  // This is only called by A-MAS (periodically) when in MANUAL and maybe P.O.V. modes.
  // Checks to see if operator pressed a "throw turnout" button on the control panel, and executes if so.
  // Check status of pin 8 (coming from A-BTN) and see if it's been pulled LOW, indicating button was pressed.
  if ((digitalRead(PIN_REQ_TX_A_BTN_IN)) == HIGH) {

    sprintf(lcdString, "%.20s", "Line is high.");
    myLCD->send(lcdString);

    return 0;  // A_BTN is not asking to send us a button press message
  
  } else {     // A_BTN is asking to send us a button press message!

    sprintf(lcdString, "%.20s", "Line went low!");
    myLCD->send(lcdString);

    byte msgBuffer[RS485_MAX_LEN];  // Since we aren't passed msgIncoming or msgOutgoing, create a temp outgoing message buffer.
    //this->setLen(msgBuffer, 5);
    //this->setTo(msgBuffer, ARDUINO_BTN);
    //this->setFrom(msgBuffer, ARDUINO_MAS);
    //this->setType(msgBuffer, 'B');  // Button number request

    setLen(msgBuffer, 5);
    setTo(msgBuffer, ARDUINO_BTN);
    setFrom(msgBuffer, ARDUINO_MAS);
    setType(msgBuffer, 'B');  // Button number request
// DEBUG DELAY
delay(500);
    //this->send(msgBuffer);
    send(msgBuffer);

    sprintf(lcdString, "%.20s", "Sent request to BTN");
    myLCD->send(lcdString);

    // Now wait for a reply...
    // There is NO legitimate reason why we would get ANY RS485 message from anyone except A-BTN at this point.
    msgBuffer[RS485_TO_OFFSET] = 0;   // Set "to" field to anything other than ARDUINO_MAS
    do {
      //this->receive(msgBuffer);  // Only returns with data if it has a complete new message
      receive(msgBuffer);  // Only returns with data if it has a complete new message
    } while (msgBuffer[RS485_TO_OFFSET] != ARDUINO_MAS);
    // We should NEVER get a message that isn't to A-MAS from A-BTN, but we'll check anyway...
    //if ((this->getTo(msgBuffer) !=  ARDUINO_MAS) || (this->getFrom(msgBuffer) != ARDUINO_BTN)) {
    if ((getTo(msgBuffer) != ARDUINO_MAS) || (getFrom(msgBuffer) != ARDUINO_BTN)) {
      sprintf(lcdString, "%.20s", "RS485 to/from error!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    
    //byte buttonNum = this->getTurnoutButtonNum(msgBuffer);   // Button number 1..32
    byte buttonNum = getTurnoutButtonNum(msgBuffer);   // Button number 1..32
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

// ***** PRIVATE METHODS *****

void Message_MAS::send(byte tMsg[]) {

  Message_RS485::RS485SendMessage(tMsg);
  return;

}

byte Message_MAS::getTurnoutButtonNum(byte tMsg[]) {
  return tMsg[RS485_MAS_BTN_BUTTON_NUM_OFFSET];
}

