// Rev: 07/07/18
// Message_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

// This class does *not* contain the byte-level/field-level knowledge of RS485 messages specific to each module.
// However, it does know the byte locations of the fields for LENGTH, TO, FROM, and TYPE, and handles checksums.

#ifndef MESSAGE_485_H
#define MESSAGE_485_H

#include <Train_Consts_Global.h>
#include <Display_2004.h>  // WE SHOULD SEE IF WE CAN JUST FORWARD-DECLARE THIS SINCE WE'RE ONLY USING A POINTER TO THE LCD

class Message_RS485
{
  public:

    Message_RS485(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004);
    // Constructor
  
    void InitPort();
    // Initializes the serial port, which was identified in the constructor

    bool RS485GetMessage(byte tMsg[]);
    // RS485GetMessage returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.

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
    
    HardwareSerial * mySerial;             // Pointer to the hardware serial port that we want to use.
    long unsigned int myBaud;              // Baud rate for serial port i.e. 9600 or 115200
    Display_2004 * myLCD;                  // Pointer to the 20x04 LCD display, used to display message processing errors and status.
    byte calcChecksumCRC8(const byte data[], byte len);

  private:

};

extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void endWithFlashingLED(int numFlashes);

#endif