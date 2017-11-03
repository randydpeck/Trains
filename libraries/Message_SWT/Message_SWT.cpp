// Child1 class.

#include "Message_SWT.h"

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_SWT::Message_SWT(byte moduleID) : Message_RS485(moduleID)
{
   _myModuleID = moduleID;
   messageRS485(_myModuleID);
}


void Message_SWT::method11()
{
   // Items must be PUBLIC or PROTECTED in the parent class to 
   // be visible to derived classes.

   // A derived class can call a visible parent method as needed.
   method1();

   return;
}


void Message_SWT::method12()
{
   // A derived class can access visible parent data fields.
   if(myModuleID > 1) childDataField--;
  
   return;
}