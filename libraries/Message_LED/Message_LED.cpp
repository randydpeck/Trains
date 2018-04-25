// Message_LED is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 04/20/18

#include <Message_LED.h>

// A_LED ??? sends and receives RS485 messages, and also needs to read a digital signal.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_LED::Message_LED(Display_2004 * LCD2004) : Message_RS485(myModuleID, myLCD)
{

  myModuleID = THIS_MODULE;  // This could probably simply be set as const byte myModuleID = ARDUINO_BTN (global const)
                             // After all, why pass it if this child class is unique to this module?

  myLCD = LCD2004;  // Thinking we may not want to be writing messages to the LCD from inside this class...???
                    // MessageRS485(myModuleID, myLCD);
}

bool Message_LED::Receive(byte tMsg[]) {

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.

  // First see if there is a valid message of any type...
  if ((Message_RS485::RS485GetMessage(tMsg)) == false) return false;

  // Okay, there was a real message found and it's stored in tMsg[].
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_MSG cares about these incoming messages (which is at this time, ALL incoming messages):
  //   A_xxx to ALL Mode broadcase
  //   A_xxx to A_BTN "send the button number that was just pressed."
  //   ********
  // THIS IS WRONG

  if ((tMsg[RS485_TO_OFFSET] == ARDUINO_ALL) && (tMsg[RS485_FROM_OFFSET] == ARDUINO_MAS) && (tMsg[RS485_TYPE_OFFSET] == 'M')) {
    return true;  // It's a Mode change broadcast message
  } else   if ((tMsg[RS485_TO_OFFSET] == ARDUINO_ALL) && (tMsg[RS485_FROM_OFFSET] == ARDUINO_MAS) && (tMsg[RS485_TYPE_OFFSET] == 'B')) {
    return true;  // It's a request from A_MAS to send the button number of the control panel button just pressed
  } else {  // Not a message we are interested in
    return false;
  }

}

void Message_LED::Send(byte tMsg[]) {

  Message_RS485::RS485SendMessage(tMsg);
  return;
 
}

/*
// This is copied from Message_BTN as an example of setting a particular field in the message byte array...
void Message_MAS::SetButtonNo(byte tMsg[], byte button) {  // Inserts button number into the appropriate byte
  tMsg[RS485_BUTTON_NO_OFFSET] = button;
  return;
}
*/