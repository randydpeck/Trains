// Train_Fns_Vars_Global declares and defines all functions and variables that are global to all (or nearly all) Arduino modules.
// Rev: 11/05/17
// extern is optional for function prototypes, or anywhere that it is obvious to the compiler that it's a declaration and not a definition.
/*

#ifndef _TRAIN_FNS_VARS_GLOBAL_H
#define _TRAIN_FNS_VARS_GLOBAL_H

#include "Arduino.h"
#include <Train_Consts_Global.h>

// *** SERIAL LCD DISPLAY CLASS:
#include <Display_2004.h>                // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
extern Display_2004 LCD2004;  // Instantiate 2004 LCD display "LCD2004."
extern Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.

extern       byte RS485MsgIncoming[];  // No need to initialize contents
extern       byte RS485MsgOutgoing[];
extern       char lcdString[];    // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

extern void chirp();
extern void requestEmergencyStop();


#endif
*/
