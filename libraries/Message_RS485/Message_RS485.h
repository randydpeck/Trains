// Rev: 07/14/18
// Message_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

// This class does *not* contain the byte-level/field-level knowledge of RS485 messages specific to each module.
// However, it does know the byte locations of the fields for LENGTH, TO, FROM, and TYPE, and handles checksums.

// 7/13/18: Need to modify to include incoming & outgoing message buffers as public attributes of class so they'll be created when we instantiate this single object *************
// Then we shouldn't need to constantly pass tMsg[] back and forth from Message_XXX, right?  Not sure how that will affect use of "this" if there are two buffers...***************

#ifndef MESSAGE_485_H
#define MESSAGE_485_H

#include <Train_Consts_Global.h>
#include <Display_2004.h>  // WE SHOULD SEE IF WE CAN JUST FORWARD-DECLARE THIS SINCE WE'RE ONLY USING A POINTER TO THE LCD *******************************************************

class Message_RS485
{
  public:

    Message_RS485(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * myLCD);  // Constructor

    bool RS485GetMessage(byte tMsg[]);
    // RS485GetMessage returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.

    void RS485SendMessage(byte tMsg[]);
  
    byte getLen(byte tMsg[]);   // Returns the 1-byte length of the RS485 message in tMsg[]

    byte getTo(byte tMsg[]);    // Returns the 1-byte "to" Arduino ID

    byte getFrom(byte tMsg[]);  // Returns the 1-byte "from" Arduino ID

    char getType(byte tMsg[]);  // Returns the 1-char message type i.e. 'M' for Mode

    void setLen(byte tMsg[], byte len);    // Inserts message length i.e. 7 into the appropriate byte

    void setTo(byte tMsg[], byte to);      // Inserts "to" i.e. ARDUINO_BTN into the appropriate byte

    void setFrom(byte tMsg[], byte from);  // Inserts "from" i.e. ARDUINO_MAS into the appropriate byte

    void setType(byte tMsg[], char type);  // Inserts the message type i.e. 'M'ode into the appropriate byte

  protected:
    
    Display_2004 * myLCD;                  // Pointer to the 20x04 LCD display, used to display message processing errors and status.

  private:

    HardwareSerial * mySerial;             // Pointer to the hardware serial port that we want to use.
    long unsigned int myBaud;              // Baud rate for serial port i.e. 9600 or 115200

    byte calcChecksumCRC8(const byte data[], byte len);

};

extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void endWithFlashingLED(int numFlashes);

#endif