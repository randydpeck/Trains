// Command_TST is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 04/20/18

#include <Command_TST.h>

// xxxxxxxxxx  Because A_SWT only receives messages and does not send any, this class does not include any "send" logic.
// xxxxxxxxxxxx Because A_SWT does not send or receive any digital messages (i.e. Arduino pins), this class does not include that logic either.
// A_BTN both sends and receives RS485 messages, and also needs to send a digital signal.

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Command_TST::Command_TST(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) : Command_RS485(hdwrSerial, baud, LCD2004) {

  mySerial = hdwrSerial;  // Pointer to the serial port we want to use for RS485 (probably not needed here)
  myBaud = baud;          // Serial port baud rate (probably not needed here)
  myLCD = LCD2004;        // Pointer to the LCD display for error messages (possibly not needed here)
  
}

void Command_TST::Init() {

  Command_RS485::InitPort();
  return;

}

bool Command_TST::Receive(byte tMsg[]) {

  // Try to retrieve a complete incoming message.  
  // Return false if 1) There was no message; or 2) The message is not one that this module cares about.
  // Return true if there was a complete message *and* it's one that we want to see.

  // First see if there is a valid message of any type...
  if ((Command_RS485::RS485GetMessage(tMsg)) == false) return false;

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

void Command_TST::Send(byte tMsg[]) {

  Command_RS485::RS485SendMessage(tMsg);
  return;
 
}

void Command_TST::SetButtonNo(byte tMsg[], byte button) {  // Inserts button number into the appropriate byte
  tMsg[RS485_BUTTON_NO_OFFSET] = button;
  return;
}
