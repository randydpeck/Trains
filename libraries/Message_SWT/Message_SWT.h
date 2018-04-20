// Rev: 04/20/18
// Message_SWT is a child class of Message_RS485.

// *** RS485 MESSAGE PROTOCOLS used by A-SWT  (similar to A-LED.)  Byte numbers represent offsets, so they start at zero. ***

// A-MAS to A-SWT:  Command to set an individual turnout
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 6
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  [N|R] Normal|Reverse
//    4    Parameter Byte  Turnout number 1..32
//    5	   Checksum	 Byte  0..255

// A-MAS to A-SWT:  Command to set a Route (regular, or Park 1 or Park 2)
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 6
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  [T|1|2] rouTe|park 1|park 2
//    4    Parameter Byte  Route number from one of the three route tables, 1..70, 1..19, or 1..4
//    5	   Checksum	 Byte  0..255

// A-MAS to A-SWT:  Command to set all turnouts to Last-known position
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 9
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  'L' = Last-known position
//    4..7 Parameter Byte  32 bits: 0=Normal, 1=Reverse
//    8	   Checksum  Byte  0..255

#ifndef _MESSAGE_SWT_h
#define _MESSAGE_SWT_h

#include <Message_RS485.h>
#include <Train_Consts_Global.h>

const byte RS485_LAST_KNOWN_OFFSET = 4;    // Offset into RS485 message where 32 bits of turnout positioning are stored

class Message_SWT : public Message_RS485 {

  public:

    // Constructor:
    Message_SWT(Display_2004 * LCD2004);
      
    // Receive returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.
    bool Receive(byte tMsg[]);

  private:

};

#endif
