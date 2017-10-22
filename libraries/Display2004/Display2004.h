// Display2004.h

#ifndef _DISPLAY2004_h
#define _DISPLAY2004_h

#include "Arduino.h"
// The following lines are required by the Digole serial LCD display, connected to serial port 1.
// The Digole LCD is an object that is used by our primary 20x04 LCD object "Display2004."
#define _Digole_Serial_UART_    // To tell compiler compile the serial communication only
#include <DigoleSerial.h>

// Not sure why I need to define this outside the closing curly braces, in order for it to be "seen" by the calling .ino...
// Seems like it could have been put in the "private:" section, but that doesn't work.
const byte LCD_WIDTH = 20;    // Number of chars wide on the 20x04 LCD displays on the control panel
void endWithFlashingLED(int numFlashes);  // Display2004 calls endWithFlashingLED and thus needs the function prototype declared here

class Display2004
{
  public:

    // Constructor declaration, note this does not specify a return type.
    // Display2004() constructor creates a new object of the type specified in the call.
    // hdwrSerial needs to be &Serial, &Serial1, &Serial2, or &Serial3 i.e. the address of the serial interface.
    // baud needs to be a valid baud rate, typically 9600 or 115200.
    // Sample call: Display2004 LCD2004(&Serial1, 115200);
    Display2004(HardwareSerial * hdwrSerial, unsigned long baud);

    // init() initializes the Digole serial LCD display, including a brief pause required before being used.
    void init();

    // send() scrolls the bottom 3 (of 4) lines up one line, and inserts the passed text into the bottom line of the LCD.
    // nextLine[] is a 20-byte max character string with a null terminator.
    // Sample call: sprintf(lcdString, "Bad Park 2 rec type!"); LCD2004.send(lcdString);
    void send(const char nextLine[]);

  protected:

  private:

    DigoleSerialDisp * _display;  // A member field in the Display2004 class that holds a pointer to a Digole object.

};

// extern Display2004 Display2004();  // What does this do???  Visual Studio added it as part of the default "new class" setup

#endif

