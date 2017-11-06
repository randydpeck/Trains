// Display_2004 handles display of messages from the modules to the 20-char, 4-line (2004) Digole LCD display.
// Rev: 11/04/17

#include <Display_2004.h>

Display_2004::Display_2004(HardwareSerial * hdwrSerial, unsigned long baud) {  // This is the constructor; not called until an instance is created

  // hdwrSerial needs to be the address of Serial thru Serial3, and baud can be any legit baud rate such as 115200.
  _display = new DigoleSerialDisp(hdwrSerial, baud);  // _display is a pointer to a DigoleSerialDisp type (which is itself an object)
  return;

}

void Display_2004::init() {

  _display->begin();                     // Required to initialize LCD
  _display->setLCDColRow(LCD_WIDTH, 4);  // Maps starting RAM address on LCD (if other than 1602)
  _display->disableCursor();             // We don't need to see a cursor on the LCD
  _display->backLightOn();
  delay(20);                              // About 15ms required to allow LCD to boot before clearing screen
  _display->clearScreen();               // FYI, won't execute as the *first* LCD command
  delay(100);                             // At 115200 baud, needs > 90ms after CLS before sending text.  No delay needed at 9600 baud.
  return;

}

void Display_2004::send(const char nextLine[]) {

  // Display a line of information on the bottom line of the 2004 LCD display on the control panel, and scroll the old lines up.
  // Rev 10/20/17: Converted to OOP and rolled Digole class into this class.
  // INPUTS: nextLine[] is a char array, must be less than 20 chars plus null or system will trigger fatal error.
  // The char arrays that hold the data are 21 bytes long, to allow for a 20-byte text message plus null terminator.
  // Sample calls:
  //   char lcdString[LCD_WIDTH + 1];  // Global array to hold strings sent to Digole 2004 LCD
  //   LCD2004.send("A-SWT Ready!");   Note: more than 20 characters will crash!
  //   sprintf(lcdString, "%.20s", "Hello world."); // 20 is hard-coded since don't know how to format with const LCD_WIDTH
  //   LCD2004.send(lcdString);   i.e. "Hello world."
  //   int a = 7; unsigned long t = millis(); char c = 'R';
  //   sprintf(lcdString, "I %3i T %6lu C %3c", a, t, c);  Will also crash if longer than 20 chars!
  //   LCD2004.send(lcdString);   i.e. "I...7.T...3149.C...R"

  // These static strings hold the current LCD display text.
  static char lineA[21] = "                    ";  // Only 20 spaces because the compiler will
  static char lineB[21] = "                    ";  // add a null at end for total 21 bytes.
  static char lineC[21] = "                    ";
  static char lineD[21] = "                    ";
  // If the incoming char array (string) is longer than the 21-byte array (20 chars plus null), then we will
  // have stepped on memory and must declare a fatal programming error.
  if ((nextLine == (char *)NULL) || (strlen(nextLine) > LCD_WIDTH)) endWithFlashingLED(13);
  // Scroll all lines up to make room for the new bottom line.
  strcpy(lineA, lineB);
  strcpy(lineB, lineC);
  strcpy(lineC, lineD);
  strncpy(lineD, nextLine, LCD_WIDTH);         // Copy the new bottom line into lineD, padded to 20 chars with nulls.
  int newLineLen = strlen(lineD);    // Get the length of the new bottom line (to the first null char.)
  // Pad the new bottom line with trailing spaces as needed.
  while (newLineLen < LCD_WIDTH) lineD[newLineLen++] = ' ';  // Last byte not touched; always remains "null."
  // Update the display.  Updated 10/28/16 by TimMe to add delays to fix rare random chars on display.
  _display->setPrintPos(0, 0);
  _display->print(lineA);
  delay(1);
  _display->setPrintPos(0, 1);
  _display->print(lineB);
  delay(2);
  _display->setPrintPos(0, 2);
  _display->print(lineC);
  delay(3);
  _display->setPrintPos(0, 3);
  _display->print(lineD);
  delay(1);
  return;

}
