// Message_RS485 is the base class that handles both RS485 and digital-pin communications.
// Rev: 11/04/17
// This class does *not* contain the byte-level/field-level knowledge of each RS485 message.
// Rather, it knows how to calculate and verify a checksum, and send and receive an RS485 serial message.
// It also knows how to set digital pins on the Arduino Mega high and low.

#ifndef _MESSAGE_485_H
#define _MESSAGE_485_H

#include <Arduino.h>
#include <Display_2004.h>
#include <Train_Consts_Global.h>
#include <Train_Fns_Vars_Global.h>

class Message_RS485
{
   public:
   
   Message_RS485(byte moduleID, Display_2004 * LCD2004);  // Constructor
//   bool Message_RS485::RS485GetMessage(byte tMsg[]);
//   byte Message_RS485::calcChecksumCRC8(const byte data[], byte len);

   bool RS485GetMessage(byte tMsg[]);
   byte calcChecksumCRC8(const byte data[], byte len);
   
   protected:
   byte myModuleID;     // A sample parent data field.
   Display_2004 * myLCD;
   const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
   const byte PIN_RS485_TX_LED = 5;      // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
   const byte PIN_RS485_RX_LED = 6;      // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
   const byte RS485_MAX_LEN = 20;        // buffer length to hold the longest possible RS485 message.  Just a guess.

   private:


};

#endif