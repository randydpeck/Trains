// Rev: 02/04/17.  Demo the quad alphanumeric display LED backpack kit.
// This version is just a simple "display up to 8 chars."  No scrolling etc.
// Use the solder jumpers on the board to set each board's address in the
// range 0x70 thru 0x77, for up to 8 four-char displays.

// IMPORTANT: Decimal places are just a little tricky.  They don't display
// as a separate character, and  you wouldn't want them to!
// There is a third, optional argument to writeDigitAscii. If you pass true,
// it will light the decimal point to the lower right of the character. if
// you pass false (the default), the decimal point will remain off. 
// https://forums.adafruit.com/viewtopic.php?f=47&t=60051

#include <Wire.h>
#include "Adafruit_LEDBackpack.h"   // Required for alphanumeric displays on control panel.
// Important: #include "Adafruit_GFX.h" is contained in Adafruit_LEDBackpack.h, so we don't need an #include statement
// here, but we do need Adafruit_GFX.h to be in our library.

Adafruit_AlphaNum4 alpha4Left = Adafruit_AlphaNum4();    // Left-most 4 characters
Adafruit_AlphaNum4 alpha4Right = Adafruit_AlphaNum4();   // Right-most 4 characters

char alphaString[9];  // Global array to hold strings sent to alpha display.  8 chars plus \n newline.

void setup() {
  Serial.begin(115200);
  alpha4Left.begin(0x70);  // pass in the address
  alpha4Left.clear();
  alpha4Right.begin(0x71);  // pass in the address
  alpha4Right.clear();
}

void loop() {
  
  sprintf(alphaString, "ABC");
  sendToAlpha(alphaString);
  delay(500);
  sprintf(alphaString, "XY  2794");
  sendToAlpha(alphaString);
  delay(500);
  sprintf(alphaString, " QST1");
  sendToAlpha(alphaString);
  delay(500);

  sendToAlpha("123");
  delay(500);
  sendToAlpha("ER  3624");
  delay(500);
  sendToAlpha(" EGDS");
  delay(500);

}

// ********* FUNCTIONS *************

void sendToAlpha(const char nextLine[])  {
  // Rev: 02/04/17, based on sendToAlpha Rev 10/14/16 - TimMe and RDP
  // The char array that holds the data is 9 bytes long, to allow for an 8-byte text msg (max.) plus null char.
  // Sample calls:
  //   char alphaString[9];  // Global array to hold strings sent to Adafruit 8-char a/n display.
  //   sendToAlpha("WP  83");   Note: more than 8 characters will crash!
  //   sprintf(alphaString, "WP  83"); 
  //   sendToAlpha(alphaString);   i.e. "WP  83"

  if ((nextLine == (char *)NULL) || (strlen(nextLine) > 8)) endWithFlashingLED(13);
  char lineA[] = "        ";  // Only 8 spaces because compiler will add null
  for (unsigned int i = 0; i < strlen(nextLine); i++) {
    lineA[i] = nextLine[i];
  }

  // set every digit to the buffer
  alpha4Left.writeDigitAscii(0, lineA[0]);
  alpha4Left.writeDigitAscii(1, lineA[1]);
  alpha4Left.writeDigitAscii(2, lineA[2]);
  alpha4Left.writeDigitAscii(3, lineA[3]);
  alpha4Right.writeDigitAscii(0, lineA[4]);
  alpha4Right.writeDigitAscii(1, lineA[5]);
  alpha4Right.writeDigitAscii(2, lineA[6]);
  alpha4Right.writeDigitAscii(3, lineA[7]);
 
  // Update the display.
  alpha4Left.writeDisplay();
  alpha4Right.writeDisplay();
}

void endWithFlashingLED(int numFlashes) {
  Serial.println("Error!");
  while (true) {}
}


