// Class Message_RS485 - this class is the base class.

#ifndef _MESSAGE485_H
#define _MESSAGE485_H

#include <Arduino.h>

class Message_RS485
{
   public:
   // Constructor:
   Message_RS485(byte moduleID);

   // Operation methods:
   void method1();
   void method2();

   protected:
   byte myModuleID;     // A sample parent data field.
};

#endif