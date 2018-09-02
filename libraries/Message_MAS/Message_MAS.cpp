// Rev: 07/17/18
// Message_MAS is a child class of Message_RS485, and handles all RS485 and digital pin messages for A_MAS.ino.
// A_MAS both sends and receives RS485 messages, and also needs to receive digital signals from A_BTN, A_SNS, and A_LEG for request to send.

#include "Message_MAS.h"

// Call the parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_MAS::Message_MAS(HardwareSerial * t_hdwrSerial, long unsigned int t_baud, Display_2004 * t_LCD2004) : Message_RS485(t_hdwrSerial, t_baud, t_LCD2004) { }

// ***** PUBLIC METHODS *****

void Message_MAS::setMode(byte t_msg[], byte t_mode) {
  t_msg[RS485_ALL_MAS_MODE_OFFSET] = t_mode;
  return;
}

void Message_MAS::setState(byte t_msg[], byte t_state) {
  t_msg[RS485_ALL_MAS_STATE_OFFSET] = t_state;
  return;
}

byte Message_MAS::getTurnoutButtonPress() {  // Public method Rev. 07/12/18
  // This is only called by A-MAS (periodically) when in MANUAL and maybe P.O.V. modes.
  // Checks to see if operator pressed a "throw turnout" button on the control panel, and executes if so.
  // Check status of pin 8 (coming from A-BTN) and see if it's been pulled LOW, indicating button was pressed.
  if ((digitalRead(PIN_REQ_TX_A_BTN_IN)) == HIGH) {
    return 0;  // A_BTN is not asking to send us a button press message
  } else {     // A_BTN is asking to send us a button press message!
    byte message[RS485_MAX_LEN];
    setLen(message, 5);
    setTo(message, ARDUINO_BTN);
    setFrom(message, ARDUINO_MAS);
    setType(message, 'B');  // Button number request

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "MAS sending RS485");
m_myLCD->send(lcdString);
//delay(1000);

    send(message);

 // *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "MAS sent RS485");
m_myLCD->send(lcdString);


    // Now wait for a reply from A_BTN...
    // There is NO legitimate reason why we would get ANY RS485 message from anyone except A-BTN at this point.
    do {  } while (receive(message) == false);  // Only returns with data if it has a complete new message

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "MAS rec'd RS485");
m_myLCD->send(lcdString);
delay(1000);

    // receive() only changes m_msgIncoming if it returns true, with a message *and* one that this module cares about.
    // We should NEVER get a message that isn't to A-MAS from A-BTN, but we'll check anyway...
    if ((getTo(message) != ARDUINO_MAS) || (getFrom(message) != ARDUINO_BTN)) {
      sprintf(lcdString, "%.20s", "RS485 to/from error!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    byte buttonNum = getTurnoutButtonNum(message);   // Button number 1..32
    if ((buttonNum < 1) || (buttonNum > TOTAL_TURNOUTS)) {
      sprintf(lcdString, "%.20s", "RS485 bad button no!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // Okay, the operator pressed a control panel pushbutton to toggle a turnout, and we have the turnout number 1..32.
    return buttonNum;
  }
}

// ***** PRIVATE METHODS *****

void Message_MAS::send(byte t_msg[]) {     // Private method Rev. 07/12/18
  Message_RS485::RS485SendMessage(t_msg);
  return;
}

bool Message_MAS::receive(byte t_msg[]) {  // Private method Rev. 07/18/18
  // Try to retrieve a complete incoming message.
  // NON-DESTRUCTIVE TO BUFFER: If we receive a real message, but the calling module doesn't want it, then we will *not* overwrite t_msg[]
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true (and populates t_msg[]) if there was a complete message *and* it's one that we want to see.
  // Thus, create a temporary buffer for the message...
byte message[RS485_MAX_LEN];
  // First see if there is a valid message of any type...
  if ((Message_RS485::RS485GetMessage(message)) == false) return false;
  // Okay, there was a real message found and it's stored in message.
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // NOTE: Since this is A-MAS, it's going to care about any message type coming in, since it's the master ;-)
  // The only possibilities are: Sensor changes from A-SNS, Button presses from A-BTN, and Registration and Question data from A-OCC.
  if ((getTo(message) == ARDUINO_MAS) && (getFrom(message) == ARDUINO_SNS) && (getType(message) == 'S')) {
    memcpy(t_msg, message, RS485_MAX_LEN);
    return true;  // It's a Sensor change response from A-SNS
  }
  else   if ((getTo(message) == ARDUINO_MAS) && (getFrom(message) == ARDUINO_BTN) && (getType(message) == 'B')) {
    memcpy(t_msg, message, RS485_MAX_LEN);
    return true;  // It's a Button press response from A-BTN
  }
  else   if ((getTo(message) == ARDUINO_MAS) && (getFrom(message) == ARDUINO_OCC) && (getType(message) == 'R')) {
    memcpy(t_msg, message, RS485_MAX_LEN);
    return true;  // It's a Registration response from A-OCC
  }
  else   if ((getTo(message) == ARDUINO_MAS) && (getFrom(message) == ARDUINO_OCC) && (getType(message) == 'Q')) {
    memcpy(t_msg, message, RS485_MAX_LEN);
    return true;  // It's a Question response from A-OCC
  }
  else {  // Yikes!  Fatal error because A_MAS should never receive a message other than one of the above!
    sprintf(lcdString, "%.20s", "Unexpected message!");
    m_myLCD->send(lcdString);
    Serial.print(lcdString);
sprintf(lcdString, "getTo = %3i", (int) getTo(message));  // Master is seeing "To Arduino #4" which means incoming message is to A_BTN!  Only outgoing should be to A_BTN.*****************************
m_myLCD->send(lcdString);
sprintf(lcdString, "getFrom = %3i", (int) getFrom(message));  // Master is seeing "From Arduino #1" which means incoming message is from A_MAS!  Only outgoing should be from A_MAS. ****************
m_myLCD->send(lcdString);
sprintf(lcdString, "getType = %3c", getType(message));
m_myLCD->send(lcdString);

    endWithFlashingLED(6);
  }
  return false;  // Code should never get here
}

byte Message_MAS::getTurnoutButtonNum(byte t_msg[]) {
  return t_msg[RS485_MAS_BTN_BUTTON_NUM_OFFSET];
}

