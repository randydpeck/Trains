// Display_2004 handles display of messages from the modules to the 20-char, 4-line (2004) Digole LCD display.
// It simplifies use of the LCD display by encapsulating all of the initialization and scrolling logic within the class.
// Rev: 04/19/18

#ifndef _DISPLAY_2004_H
#define _DISPLAY_2004_H

#include "Arduino.h"
#include <Train_Consts_Global.h>

// The following lines are required by the Digole serial LCD display, connected to serial port 1.
// The Digole LCD is an object that is used by our primary 20x04 LCD object "Display_2004."
#define _Digole_Serial_UART_    // To tell compiler compile the Digole library serial communication only
#include <DigoleSerial.h>       

class Display_2004
{
  public:

    // Display_2004() constructor creates a new object of the type specified in the call.
    // hdwrSerial needs to be &Serial, &Serial1, &Serial2, or &Serial3 i.e. the address of the serial interface.
    // baud needs to be a valid baud rate, typically 9600 or 115200.
    // Sample call: Display_2004 LCD2004(&Serial1, 115200);
    Display_2004(HardwareSerial * hdwrSerial, unsigned long baud);

    // init() initializes the Digole serial LCD display, including a brief pause required before being used.
    void init();

    // send() scrolls the bottom 3 (of 4) lines up one line, and inserts the passed text into the bottom line of the LCD.
    // nextLine[] is a 20-byte max character string with a null terminator.
    // Sample call: sprintf(lcdString, "Bad Park 2 rec type!"); LCD2004.send(lcdString);
    void send(const char nextLine[]);

  protected:

  private:

    // DigoleSerialDisp is the name of the class in DigoleSerial.h/.cpp.
    DigoleSerialDisp * _display;  // A member field in the Display_2004 class that holds a pointer to a Digole object.

};

extern void endWithFlashingLED(int numFlashes); // This function must be in the main .ino calling program

#endif

