#include <Train_Consts_Global.h>
char APPVERSION[LCD_WIDTH + 1] = "A-BTN Rev. 10/01/17";
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?

// INCLUDE COMMANDS: Quotes = in the ini's directory; angle brackets = in the library directory.

// *** SERIAL LCD DISPLAY CLASS:
#define _Digole_Serial_UART_                  // To tell compiler compile the serial communication only
#include <DigoleSerial.h>                     // Tell the compiler to use the DigoleSerial class library
// DigoleSerialDisp needs serial port address i.e. &Serial, &Serial1, &Serial2, or &Serial3.
// DigoleSerialDisp also needs a valid baud rate, typically 9600 or 115200.
DigoleSerialDisp digoleLCD(&Serial1, 115200); // Instantiate and name the Digole object
#include <Display_2004.h>                     // Our LCD message-display library
Display_2004 LCD(&digoleLCD);                 // Instantiate our LCD object LCD by passing a pointer to the Digole LCD object.
char lcdString[LCD_WIDTH + 1];                // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** RS485 & DIgital Pin MESSAGE CLASS (Inter-Arduino communications):
//#include <Command_RS485.h>                    // Generic RS485 communication class, in the library directory (#included in Command_TST)
#include "Command_TST.h"                      // This module's communitcation class, in its .ini directory

Command_TST Command(&Serial2, 115200, &LCD);  // Instantiate RS485 command object.
byte MsgIncoming[RS485_MAX_LEN];              // Global array for incoming inter-Arduino messages.  No need to init contents.
byte MsgOutgoing[RS485_MAX_LEN];              // No need to initialize contents.

void setup() {

  Serial.begin(115200);                 // PC serial monitor window (MOVE THIS CONST TO A GLOBAL ie. 9600 or 115200)
  Serial.println("Starting setup().");
  LCD.init();                       // Initialize the 20 x 04 Digole serial LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  LCD.send(lcdString);
  Command.Init(); // This will init the Command object
  
}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

sprintf(lcdString, "Howdy!");
LCD.send(lcdString);
delay(1000);
}

void endWithFlashingLED(int x) {}
