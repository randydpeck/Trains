// Message_SWT is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 04/20/18

#include <Message_SWT.h>

// Because A_SWT only receives messages and does not send any, this class does not include any "send" logic.
// Because A_SWT does not send or receive any digital messages (i.e. Arduino pins), this class does not include that logic either.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_SWT::Message_SWT(Display_2004 * LCD2004) : Message_RS485(myModuleID, myLCD)
{

  myModuleID = THIS_MODULE;  // This could probably simply be set as const byte myModuleID = ARDUINO_SWT (global const)
                             // After all, why pass it if this child class is unique to this module?

  myLCD = LCD2004;  // Thinking we may not want to be writing messages to the LCD from inside this class...???

}

bool Message_SWT::Receive(byte tMsg[]) {

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.

  // First see if there is a valid message of any type...
  if ((Message_RS485::RS485GetMessage(tMsg)) == false) return false;

  // Okay, there was a real message found and it's stored in tMsg[].
  // Decide if it's a message that this module even cares about, based on From, To, and Message type.
  // A_SWT only cares about three messages -- each of them from A_MAS to A_SWT.  Doesn't care about Mode broadcast.

  if (tMsg[RS485_TO_OFFSET] == ARDUINO_SWT) {  // Any and only a message "to" us is good to go...
    return true;
  }
  else {
    return false;
  }

}
