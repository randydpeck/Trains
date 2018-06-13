// Rev: 05/06/18
// Network_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

// This class does *not* contain the byte-level/field-level knowledge of RS485 messages specific to each module.
// However, it does know the byte locations of the fields for LENGHT, TO, FROM, and TYPE, and handles checksums.

#ifndef NETWORK_485_H
#define NETWORK_485_H

#include "Train_Consts_Global.h"
#include "Display_2004.h"  // WE SHOULD SEE IF WE CAN JUST FORWARD-DECLARE THIS SINCE WE'RE ONLY USING A POINTER TO THE LCD

class Network_RS485
{
  public:  // There are no public methods; these must all be called from child classes i.e. Network_BTN.h/.cpp.

  protected:  // Accessible only by child classes such as Network_BTN.h/.cpp etc.

    Network_RS485(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004);
    // Constructor
  
    void InitPort();
    // Initializes the serial port, which was identified in the constructor

    bool RS485GetMessage();
    // RS485GetMessage returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.

    void RS485SendMessage();

    byte calcChecksumCRC8(const byte data[], byte len);

    byte GetLen();   // Returns the 1-byte length of the RS485 message in tMsg[]

    byte GetTo();    // Returns the 1-byte "to" Arduino ID

    byte GetFrom();  // Returns the 1-byte "from" Arduino ID

    char GetType();  // Returns the 1-char message type i.e. 'M' for Mode

    void SetLen(byte len);    // Inserts message length i.e. 7 into the appropriate byte

    void SetTo(byte to);      // Inserts "to" i.e. ARDUINO_BTN into the appropriate byte

    void SetFrom(byte from);  // Inserts "from" i.e. ARDUINO_MAS into the appropriate byte

    void SetType(char type);  // Inserts the message type i.e. 'M'ode into the appropriate byte

    byte MsgBufIn[RS485_MAX_LEN];
    byte MsgBufOut[RS485_MAX_LEN];

    HardwareSerial * m_Serial;             // Pointer to the hardware serial port that we want to use.
    long unsigned int m_Baud;              // Baud rate for serial port i.e. 9600 or 115200
    Display_2004 * m_LCD;                  // Pointer to the 20x04 LCD display, used to display message processing errors and status.

  private:

};

extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void endWithFlashingLED(int numFlashes);

#endif