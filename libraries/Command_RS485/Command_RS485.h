// Rev: 05/03/18
// Command_RS485 handles RS485 (and maybe? digital-pin) communications, via a specified serial port.

// This class is not called directly by A_xxx.ino, except MAYBE??? to instantiate and init it.
// Somehow we need to init the serial port for this module, not needed by the child classes.

// It's purpose is to be called by the various module's Command objects, i.e. Command_SWT.
// This class does *not* contain the byte-level/field-level knowledge of each RS485 message.
// Rather, it knows how to calculate and verify a checksum, and send and receive an RS485 serial message.

#ifndef _COMMAND_485_H
#define _COMMAND_485_H

#include <Arduino.h>
#include "Train_Consts_Global.h"
#include "Display_2004.h"  // WE SHOULD SEE IF WE CAN JUST FORWARD-DECLARE THIS SINCE WE'RE ONLY USING A POINTER TO THE LCD

class Command_RS485
{
  public:

    Command_RS485(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004);  // Constructor
  
    void InitPort();  // Initializes the serial port, which was identified in the constructor

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

  protected:
    
    // Here is where we can start adding support for digital pins...somehow.  Probably at least part needs to be public, not protected, though.
    // const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
    // const byte PIN_RS485_TX_LED = 5;      // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
    // const byte PIN_RS485_RX_LED = 6;      // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
    // const byte RS485_MAX_LEN = 20;        // buffer length to hold the longest possible RS485 message.  Just a guess.
    HardwareSerial * mySerial;            // Pointer to the hardware serial port that we want to use.
    long unsigned int myBaud;             // Baud rate for serial port i.e. 9600 or 115200
    Display_2004 * myLCD;                 // Pointer to the 20x04 LCD display, used to display message processing errors and status.
    byte calcChecksumCRC8(const byte data[], byte len);

  private:

};

extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void endWithFlashingLED(int numFlashes);

#endif