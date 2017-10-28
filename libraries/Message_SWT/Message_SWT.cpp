// Child1 class.

#include "Child1.h"

// Here is how you call the Parent constructor from the child constructor, passing it the parameter(s) it needs.
Child1::Child1(byte moduleID) : Parent(moduleID)
{
   childDataField = 0;
}


void Child1::method11()
{
   // Items must be PUBLIC or PROTECTED in the parent class to 
   // be visible to derived classes.

   // A derived class can call a visible parent method as needed.
   method1();

   return;
}


void Child1::method12()
{
   // A derived class can access visible parent data fields.
   if(myModuleID > 1) childDataField--;
  
   return;
}