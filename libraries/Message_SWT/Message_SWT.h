// Message_SWT is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 11/04/17

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-SWT  (same as A-LED.)  Byte numbers represent offsets, so they start at zero. ***
// Rev: 11/06/17

// A-MAS BROADCAST: Mode change
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//   	0	  Length	  Byte	7
//   	1	  To	      Byte	99 (ALL)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'M' means this is a Mode/State update command
//   	4	  Mode	    Byte  1..5 [Manual | Register | Auto | Park | POV]
//   	5	  State	    Byte  1..3 [Running | Stopping | Stopped]
//   	6	  Cksum	    Byte  0..255

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

// **************************************************************************************************************************


#ifndef _MESSAGE_SWT_H
#define _MESSAGE_SWT_H

#include <Message_RS485.h>
#include <Train_Consts_Global.h>
//#include <Train_Fns_Vars_Global.h>
//#include <Train_Fns_Vars_Consts_SWT.h>

class Message_SWT : public Message_RS485
{
   public:
      // Constructor:
      Message_SWT(Display_2004 * LCD2004);
      
      // Command handling methods:
      void method11();
      void method12();
   
   private:
//   byte myModuleID;   // A sample child data field.
   Display_2004 * myLCD;


};

#endif
