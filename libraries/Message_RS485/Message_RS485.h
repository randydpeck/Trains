// Message_RS485 is the base class that handles both RS485 and digital-pin communications.  
// Rev: 04/20/18
// This class is never called directly by A_xxx.ino; rather,it is called by a child class such as Message_SWT.
// This class does *not* contain the byte-level/field-level knowledge of each RS485 message.
// Rather, it knows how to calculate and verify a checksum, and send and receive an RS485 serial message.
// It also knows how to set digital pins on the Arduino Mega high and low (not yet implemented as of 4/19/18.)

// Note that the serial input buffer is only 64 bytes, which means that we need to keep emptying it since there
// will be many commands between Arduinos, even though most may not be for THIS Arduino.  If the buffer overflows,
// then we will be totally screwed up (but it will be apparent in the checksum.)

#ifndef _MESSAGE_485_H
#define _MESSAGE_485_H

#include <Arduino.h>
#include <Display_2004.h>
#include <Train_Consts_Global.h>

class Message_RS485
{
  public:

  // I feel like the following functions should be "protected" since I will only want to access them from child classes such as Message_SWT.
  // However the program won't compile if I make them protected.
  // Maybe the message object in Message_SWT isn't a child class, but simply a separate class that uses this class?

  // Constructor wants the module ID that is calling it (i.e. ARDUINO_SWT), and a pointer to the 2004 LCD display object
  Message_RS485(byte moduleID, Display_2004 * LCD2004);  // Constructor
  
  // RS485GetMessage returns true or false, depending if a complete message was read.
  // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.
  bool RS485GetMessage(byte tMsg[]);

  void RS485SendMessage(byte tMsg[]);
  
  byte GetLen(byte tMsg[]);   // Returns the 1-byte length of the RS485 message in tMsg[]

  byte GetTo(byte tMsg[]);    // Returns the 1-byte "to" Arduino ID

  byte GetFrom(byte tMsg[]);  // Returns the 1-byte "from" Arduino ID

  char GetType(byte tMsg[]);  // Returns the 1-char message type i.e. 'M' for Mode

  void SetLen(byte tMsg[], byte len);    // Inserts message length i.e. 7 into the appropriate byte

  void SetTo(byte tMsg[], byte to);      // Inserts "to" i.e. ARDUINO_BTN into the appropriate byte

  void SetFrom(byte tMsg[], byte from);  // Inserts "from" i.e. ARDUINO_MAS into the appropriate byte

  void SetType(byte tMsg[], char type);  // Inserts the message type i.e. 'M'ode into the appropriate byte

  void SetCksum(byte tMsg[], byte len);  // Inserts a checksum byte at the end of the message

  protected:

  byte myModuleID;                      // Keeps track of the module that is calling i.e. A_SWT.  Shared with children Message_XXX.h/.cpp.
  Display_2004 * myLCD;                 // Pointer to the 20x04 LCD display, used to display message processing errors and status.

  // Here is where we can start adding support for digital pins...somehow.  Probably at least part needs to be public, not protected, though.
//   const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
//   const byte PIN_RS485_TX_LED = 5;      // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
//   const byte PIN_RS485_RX_LED = 6;      // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
//   const byte RS485_MAX_LEN = 20;        // buffer length to hold the longest possible RS485 message.  Just a guess.

  private:

  byte calcChecksumCRC8(const byte data[], byte len);

};

extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void endWithFlashingLED(int numFlashes);

#endif