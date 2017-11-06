// Message_SWT is a child class of Message_RS485: it handles the construction/deconstruction of message fields
// Rev: 11/04/17

#include "Message_SWT.h"

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Message_SWT::Message_SWT(byte ID, Display_2004 * LCD2004) : Message_RS485(myModuleID, myLCD)
{

   myModuleID = ID;  // This could probably simply be set as const byte myModuleID = A_SWT (global const)
                     // After all, why pass it if this child class is unique to this module?

   myLCD = LCD2004;  // Thinking we may not want to be writing messages to the LCD from inside this class...???
   // MessageRS485(myModuleID, myLCD);
}

/*
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
*/