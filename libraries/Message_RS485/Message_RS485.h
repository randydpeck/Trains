// Class Message_RS485 - this class is the base class.

#ifndef _MESSAGE485_H
#define _MESSAGE485_H

#include <Arduino.h>
#include <Display2004.h>

class Message_RS485
{
   public:
   
   Message_RS485(byte moduleID, Display2004 * LCD2004);  // Constructor
//   bool Message_RS485::RS485GetMessage(byte tMsg[]);
//   byte Message_RS485::calcChecksumCRC8(const byte data[], byte len);

   bool RS485GetMessage(byte tMsg[]);
   byte calcChecksumCRC8(const byte data[], byte len);
   
   protected:
   byte myModuleID;     // A sample parent data field.

   private:

   Display2004 * myLCD;
   const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
   const byte PIN_RS485_TX_LED = 5;  // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
   const byte PIN_RS485_RX_LED = 6;  // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
   char lcdString[LCD_WIDTH + 1];  // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
   const byte RS485_MAX_LEN = 20;    // buffer length to hold the longest possible RS485 message.  Just a guess.

};

#endif