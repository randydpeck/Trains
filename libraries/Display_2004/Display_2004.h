// Rev: 04/22/18
// Display_2004 handles display of messages from the modules to the 20-char, 4-line (2004) Digole LCD display.
// It simplifies use of the LCD display by encapsulating all of the initialization and scrolling logic within the class.
// Note that Display_2004 is neither a parent nor a child of the DigoleSerial object; it simply uses that object.

#ifndef _DISPLAY_2004_H
#define _DISPLAY_2004_H

#include <Arduino.h>
#include <Train_Consts_Global.h>

// The following lines are required by the Digole serial LCD display, connected to an Arduino hardware serial port.
// The Digole LCD is an object that is used by our primary 20x04 LCD object "Display_2004."
#define _Digole_Serial_UART_    // To tell compiler compile the Digole library serial communication only
#include <DigoleSerial.h>       

class Display_2004
{
  public:

    Display_2004(DigoleSerialDisp * myDisp);  // Constructor.

    // init() initializes the Digole serial LCD display, including a brief pause required before being used.
    void init();

    // send() scrolls the bottom 3 (of 4) lines up one line, and inserts the passed text into the bottom line of the LCD.
    // nextLine[] is a 20-byte max character string with a null terminator.
    // Sample call: sprintf(lcdString, "Bad Park 2 rec type!"); LCD.send(lcdString);
    void send(const char nextLine[]);

  protected:

  private:

    // DigoleSerialDisp is the name of the class in DigoleSerial.h/.cpp.
    // So let's create a private pointer, called myLCD, to an object of that type (class):
    DigoleSerialDisp * myLCD;

};

extern void endWithFlashingLED(int numFlashes); // This function must be in the main .ino calling program

#endif

