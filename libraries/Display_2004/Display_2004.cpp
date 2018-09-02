// Rev: 07/14/18
// Display_2004 handles display of messages from the modules to the 20-char, 4-line (2004) Digole LCD display.

// 09/01/18: WARNING WARNING WARNING: The following constructor uses delay(), AND IT MUST NOT!  Replace with a
// microsecond timer, which does work on Arduino pre-setup().

// Also, I think I prefer method 2.
// Also, why not make this a parent/child relationship?  Although it "uses" and LCD display, rather than being "a kind of" LCD display...???
// How would we instantiate the object in that case?

// Also, my C++ Primer book shows a containment example, where the main object's constructor references an
// already-created object of the type being contained.  But what I'm doing below doesn't call the
// digole constructor; it just references a pointer to an object of that type...  I'm confused.
// Refer to the student class example in that book.
// The way I've written it below, I'm using the digole class but my 2004 object doesn't *contain* it, I think...???  Why???

#include "Display_2004.h"

//Display_2004::Display_2004(HardwareSerial * t_hdwrSerial, long unsigned int t_baud) {  // Constructor for methods 1 or 2
Display_2004::Display_2004(DigoleSerialDisp * t_digoleLCD) {  // Constructor using Method 3

  // DigoleSerialDisp is the name of the class in DigoleSerial.h/.cpp.  Note: Neither a parent nor a child class.
  // myLCD is a private pointer of type DigoleSerialDisp that will point to the Digole object.

  // There are three ways to instantiate the DigoleSerialDisp object needed by the Display_2004 constructor.
  
  // Method 1 uses the "new" keyword to create the object on the heap.  
  // The advantage is that it encapsulates the use of the DigoleSerial class, so the calling .ino doesn't even know about it.
  // Also, it creates a new object each time the constructor is called, so you can have more than one LCD display.
  // The disadvantage is that use of "new" with Arduino is discouraged, because it may fragment the heap and memory is so small.
  // Constructor definition:
  //   Display_2004::Display_2004(HardwareSerial * t_hdwrSerial, long unsigned int t_baud) {
  // Constructor content:
  //   m_myLCD = new DigoleSerialDisp(t_hdwrSerial, t_baud);
  // hdwrSerial needs to be the address of Serial thru Serial3, a long unsigned int.
  // baud can be any legit baud rate such as 115200.

  // Method 2 uses the more traditional (for Arduino) way to instantiate an object, creating the object on the stack.
  // It also has the advantage of encapsulating the use of the DigoleSerial class, so the calling .ino doesn't even know about it.
  // The disadvantage of Method 2 is that, by using static here, you can only have *one* LCD display object.  Which is fine in this case.
  // Constructor definition:
  //   Display_2004::Display_2004(HardwareSerial * t_hdwrSerial, long unsigned int t_baud) {
  // Constructor content:
  //   static DigoleSerialDisp DigoleLCD(t_hdwrSerial, t_baud);
  //   m_myLCD = &DigoleLCD;

  // Method 3 (USED HERE) requires you to instantiate the DigoleSerialDisp object in the calling .ino code, instead of this constructor.
  // It overcomes the disadvantages of both above methods: It does not use "new", and it allows instantiation of multiple objects.
  // The disadvantage is that you need #define and #include statements about DigoleSerial in the calling .ino program, so the
  // Digole class is no longer encapsulated in the Display_2004 class.
  // The calling .ino program would need to following lines:
  // #define _Digole_Serial_UART_  // To tell compiler compile the serial communication only
  // #include "DigoleSerial.h"     // Tell the compiler to use the DigoleSerial class library
  // DigoleSerialDisp digoleLCD(&Serial1, (long unsigned int) 115200); // Instantiate and name the Digole object
  // #include "Display_2004.h"                // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
  // Display_2004 LCD(&digoleLCD);  // Instantiate our LCD object called LCD, pass pointer to DigoleLCD
  // And the constructor definition would simply require a pointer to the Digole object:
  //   Display_2004::Display_2004(DigoleSerialDisp * digoleLCD) {  // Constructor for method 3
  // And the constructor would need this line:

  m_myLCD = t_digoleLCD;  // METHOD 3: Assigns the Digole LCD pointer we were passed to our local private pointer

  // 07/14/18: Can't be part of constructor if it's called above init(), since constructor is called before Setup(), and you can't use delay() *or* Serial.begin() before Setup() on Arduino.
  // However in this case, we're calling the constructor at the top of loop(), so it's no problem.
  m_myLCD->begin();                     // Required to initialize LCD
  m_myLCD->setLCDColRow(LCD_WIDTH, 4);  // Maps starting RAM address on LCD (if other than 1602)
  m_myLCD->disableCursor();             // We don't need to see a cursor on the LCD
  m_myLCD->backLightOn();
  delay(30);                           // About 15ms required to allow LCD to boot before clearing screen
  m_myLCD->clearScreen();               // FYI, won't execute as the *first* LCD command
  delay(100);                          // At 115200 baud, needs > 90ms after CLS before sending text.  No delay needed at 9600 baud.
  return;


}

/* void Display_2004::init() {

  // 07/14/18: Can't be part of constructor since constructor is called before Setup(), and you can't use delay() before Setup() on Arduino.
  m_myLCD->begin();                     // Required to initialize LCD
  m_myLCD->setLCDColRow(LCD_WIDTH, 4);  // Maps starting RAM address on LCD (if other than 1602)
  m_myLCD->disableCursor();             // We don't need to see a cursor on the LCD
  m_myLCD->backLightOn();
  delay(30);                           // About 15ms required to allow LCD to boot before clearing screen
  m_myLCD->clearScreen();               // FYI, won't execute as the *first* LCD command
  delay(100);                          // At 115200 baud, needs > 90ms after CLS before sending text.  No delay needed at 9600 baud.
  return;

} */

void Display_2004::send(const char t_nextLine[]) {
  // Display a line of information on the bottom line of the 2004 LCD display on the control panel, and scroll the old lines up.
  // Rev 10/20/17: Converted to OOP and rolled Digole class into this class.
  // INPUTS: nextLine[] is a char array, must be less than 20 chars plus null or system will trigger fatal error.
  // The char arrays that hold the data are 21 bytes long, to allow for a 20-byte text message plus null terminator.
  // Sample calls:
  //   char lcdString[LCD_WIDTH + 1];  // Global array to hold strings sent to Digole 2004 LCD
  //   LCD.send("A-SWT Ready!");   Note: more than 20 characters will crash!
  //   sprintf(lcdString, "%.20s", "Hello world."); // 20 is hard-coded since don't know how to format with const LCD_WIDTH
  //   LCD.send(lcdString);   i.e. "Hello world."
  //   int a = 7; unsigned long t = millis(); char c = 'R';
  //   sprintf(lcdString, "I %3i T %6lu C %3c", a, t, c);  Will also crash if longer than 20 chars!
  //   LCD.send(lcdString);   i.e. "I...7.T...3149.C...R"

  // These static strings hold the current LCD display text.
  static char lineA[21] = "                    ";  // Only 20 spaces because the compiler will
  static char lineB[21] = "                    ";  // add a null at end for total 21 bytes.
  static char lineC[21] = "                    ";
  static char lineD[21] = "                    ";
  // If the incoming char array (string) is longer than the 21-byte array (20 chars plus null), then we will
  // have stepped on memory and must declare a fatal programming error.
  if ((t_nextLine == (char *)NULL) || (strlen(t_nextLine) > LCD_WIDTH)) endWithFlashingLED(13);
  // Scroll all lines up to make room for the new bottom line.
  strcpy(lineA, lineB);
  strcpy(lineB, lineC);
  strcpy(lineC, lineD);
  strncpy(lineD, t_nextLine, LCD_WIDTH);         // Copy the new bottom line into lineD, padded to 20 chars with nulls.
  int newLineLen = strlen(lineD);    // Get the length of the new bottom line (to the first null char.)
  // Pad the new bottom line with trailing spaces as needed.
  while (newLineLen < LCD_WIDTH) lineD[newLineLen++] = ' ';  // Last byte not touched; always remains "null."
  // Update the display.  Updated 10/28/16 by TimMe to add delays to fix rare random chars on display.
  m_myLCD->setPrintPos(0, 0);
  m_myLCD->print(lineA);
  delay(1);
  m_myLCD->setPrintPos(0, 1);
  m_myLCD->print(lineB);
  delay(2);
  m_myLCD->setPrintPos(0, 2);
  m_myLCD->print(lineC);
  delay(3);
  m_myLCD->setPrintPos(0, 3);
  m_myLCD->print(lineD);
  delay(1);
  return;

}
