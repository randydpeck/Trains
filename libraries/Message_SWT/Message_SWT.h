// Message_SWT is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 11/04/17

#ifndef _MESSAGE_SWT_H
#define _MESSAGE_SWT_H

#include "Message_RS485.h"

class Message_SWT : public Message_RS485
{
   public:
      // Constructor:
      Message_SWT(byte moduleID, Display_2004 * LCD2004);
      
      // Command handling methods:
      void method11();
      void method12();
   
   private:
//      byte myModuleID;   // A sample child data field.
   byte myModuleID;     // A sample parent data field.
   Display_2004 * myLCD;


};

#endif
