// Rev: 07/16/18
// Message_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

// 09-06-18: IMPORTANT IMPORTANT IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// RS 485 is not really a parent class. The base class should define qualities common to all objects derived from the base. I’m just
// thinking that digital lines are not related to 485.  Relationship is more of a “has a“ rather than a “is a kind of“ relationship.
// Just like my display class has an LED display, yet is not a kind of LED display.


// This class does *not* contain the byte-level/field-level knowledge of RS485 messages specific to each module.
// However, it does know the byte locations of the fields for LENGTH, TO, FROM, and TYPE, and handles checksums.

#ifndef MESSAGE_485_H
#define MESSAGE_485_H

#include "Train_Consts_Global.h"
#include "Display_2004.h"

class Message_RS485
{
  public:

    Message_RS485(HardwareSerial * t_hdwrSerial, long unsigned int t_baud, Display_2004 * t_myLCD);  // Constructor

    bool RS485GetMessage(byte t_msg[]);
    // RS485GetMessage returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.

    void RS485SendMessage(byte t_msg[]);
  
    byte getLen(const byte t_msg[]);   // Returns the 1-byte length of the RS485 message in tMsg[]

    byte getTo(const byte t_msg[]);    // Returns the 1-byte "to" Arduino ID

    byte getFrom(const byte t_msg[]);  // Returns the 1-byte "from" Arduino ID

    char getType(const byte t_msg[]);  // Returns the 1-char message type i.e. 'M' for Mode

    byte getChecksum(const byte t_msg[]);  // Just retrieves a byte from an existing message; does not calculate checksum!

    void setLen(byte t_msg[], byte t_len);    // Inserts message length i.e. 7 into the appropriate byte

    void setTo(byte t_msg[], byte t_to);      // Inserts "to" i.e. ARDUINO_BTN into the appropriate byte

    void setFrom(byte t_msg[], byte t_from);  // Inserts "from" i.e. ARDUINO_MAS into the appropriate byte

    void setType(byte t_msg[], char t_type);  // Inserts the message type i.e. 'M'ode into the appropriate byte

  protected:
    
    Display_2004 * m_myLCD;                  // Pointer to the 20x04 LCD display, used to display message processing errors and status.
    // Display_2004 is the name of our LCD class.  Create a private pointer, called myLCD, to an object of that type (class.)
    // Because this is a protected variable, it will be shared with child classes such as Message_BTN.
//    byte m_msgIncoming[RS485_MAX_LEN];       // Array for incoming RS485 messages.  No need to init contents.
//    byte m_msgOutgoing[RS485_MAX_LEN];       // Array for outgoing RS485 messages.
//    byte m_msgScratch[RS485_MAX_LEN];        // Temporarily holds message data when we don't want to clobber Incoming or Outgoing buffers.

  private:

    HardwareSerial * m_mySerial;             // Pointer to the hardware serial port that we want to use.
    long unsigned int m_myBaud;              // Baud rate for serial port i.e. 9600 or 115200

    byte calcChecksumCRC8(const byte t_msg[], byte t_len);  // Private function to calculate checksum of an incoming or outgoing message.

};

// The following declarations are shared with child classes (i.e. Message_BTN, etc.):
extern char lcdString[];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
extern void checkIfHaltPinPulledLow();

#endif