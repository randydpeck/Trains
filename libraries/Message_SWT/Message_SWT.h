// Class Child1 - this class is a derived class that knows about commands for a specific module.


#ifndef CHILD1_H
#define CHILD1_H

#include "Message_RS485.h"

class Message_SWT : public Message_RS485
{
   public:
      // Constructor:
      Message_SWT(byte  moduleID);

      // Command handling methods:
      void method11();
      void method12();
   
   private:
      byte _myModuleID;   // A sample child data field.

};

#endif
