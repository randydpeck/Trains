

void setup() { 
  pinMode(16, OUTPUT);         // PINMODE seems to have fixed my problems with phantom input appearing in the RS485 input buffers.  Sheesh.
  pinMode(17, INPUT_PULLUP);

}

void loop() {

  // Apparently Serial.begin() does *not* work inside of constructors.  Nor does delay().
  // So create an object.init() function that gets called in setup() (or at the top of loop()) instead.

  // *** SERIAL LCD DISPLAY CLASS:
  // We will create an object called "LCD" that displays messages on the 2004 display (i.e. LCD.send)  Although this object requires that we have
  // previously created a "digoleLCD" object from the DigoleSerial class library, "LCD" is not a child class/object of digoleLCD.  When we instantiate
  // our LCD object, we'll pass it a pointer to the digoleLCD object, and then we can forget about the Digole class/object altogether.
  //#define _Digole_Serial_UART_                  // Tell compiler to use only the serial part of the DigoleSerial class library
  //#include "DigoleSerial.h"                     // Tell the compiler to use the DigoleSerial class library
  //#include "Display_2004.h"                     // Our LCD message-display library; requires DigoleSerialDisp object "digoleLCD"
  //char lcdString[LCD_WIDTH + 1];                // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

  // DigoleSerialDisp needs serial port address i.e. &Serial, &Serial1, &Serial2, or &Serial3.
  // DigoleSerialDisp also needs a valid baud rate, typically 9600 or 115200.
  
  //  LCD.init();  (moved into constructor since this is now in loop()     // Init the 2004 Digole LCD object; must be done here, not in constructor, because it includes delay().

  // Serial1 is for the Digole 20x4 LCD debug display, and the port was initialized when we instantiated the dogoleLCD object, above.
  // Serial2 is for our RS485 message bus, and was initialized when we instantiated the Message object, above.

    
  
}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************
