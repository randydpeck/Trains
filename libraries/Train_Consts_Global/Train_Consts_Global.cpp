// Rev: 05/05/18
// Train_Consts_Global declares and defines all constants that are global to all (or nearly all) Arduino modules.

#include <Train_Consts_Global.h>

/*
// I think I prefer to define the following three arrays in the A_xxx.ino program code.  
// It's one thing to put named constants such as RS485_MAX_LEN and LCD_WIDTH in a "globals" file, but I'd rather define arrays,
// even if global, in the main program.
char lcdString[LCD_WIDTH + 1];         // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.
byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
byte RS485MsgOutgoing[RS485_MAX_LEN];
*/