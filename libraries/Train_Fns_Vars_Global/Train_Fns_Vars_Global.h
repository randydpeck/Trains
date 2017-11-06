// Train_Fns_Vars_Global declares and defines all functions and variables that are global to all (or nearly all) Arduino modules.
// Rev: 11/05/17

#ifndef _TRAIN_FNS_VARS_GLOBAL_H
#define _TRAIN_FNS_VARS_GLOBAL_H

#include "Arduino.h"
#include <Train_Consts_Global.h>

extern char lcdString[];

extern void initializeShiftRegisterPins();       // This might be different depending on if the Centipede is input vs output, and 1 or 2 boards
extern void endWithFlashingLED(int numFlashes);  // Global function prototype
extern void chirp();
extern void requestEmergencyStop();

#endif

