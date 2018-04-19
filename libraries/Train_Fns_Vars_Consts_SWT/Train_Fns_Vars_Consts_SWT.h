// Train_Fns_Vars_Consts_SWT contains all functions, variables, and constants that are unique to A_SWT.
// It is also a child class to parent class(es) that contain fns/vars/consts that are used by more than one module.
// Rev: 11/06/17

#ifndef _TRAIN_FNS_VARS_CONSTS_SWT_H
#define _TRAIN_FNS_VARS_CONSTS_SWT_H

/*
#include "Arduino.h"
#include <avr/wdt.h>     // Required to call wdt_reset() for watchdog timer for turnout solenoids
#include <Train_Consts_Global.h>
#include <Train_Fns_Vars_Global.h

extern const byte THIS_MODULE;  // Will be set to A_MAS, A_SWT, etc. for each module

extern void checkIfHaltPinPulledLow();
extern void endWithFlashingLED(int numFlashes);
extern void initializeShiftRegisterPins();

// *** SERIAL LCD DISPLAY CLASS:
#include <Display_2004.h>                // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
extern Display_2004 LCD2004;  // Instantiate 2004 LCD display "LCD2004."
extern Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.
//extern Display_2004 LCD2004(&Serial1, 115200);  // Instantiate 2004 LCD display "LCD2004."
//extern Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.
// *** SHIFT REGISTER CLASS:
#include <Wire.h>                        // Needed for Centipede shift register.
#include <Centipede.h>                   // Also, obviously, needed for Centipede.
extern Centipede shiftRegister;                 // Instantiate Centipede shift register object "shiftRegister".

#endif
*/