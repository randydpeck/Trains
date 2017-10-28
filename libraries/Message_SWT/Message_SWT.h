// Class Child1 - this class is a derived class that knows about commands for a specific module.


#ifndef CHILD1_H
#define CHILD1_H

#include "Parent.h"

class Child1 : public Parent
{
   public:
      // Constructor:
      Child1(byte  moduleID);

      // Command handling methods:
      void method11();
      void method12();
   
   private:
      byte childDataField;   // A sample child data field.

};

#endif
