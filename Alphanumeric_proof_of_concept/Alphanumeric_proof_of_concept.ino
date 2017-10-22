// Demo the quad alphanumeric display LED backpack kit
// scrolls through every character, then scrolls Serial
// input onto the display
// 2/3/16: Updated by RDP to use two side-by-side 4-char displays
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

void setup() {
  Serial.begin(115200);
  
  alpha4Left.begin(0x70);  // pass in the address
  alpha4Right.begin(0x71);  // pass in the address

  alpha4Left.writeDigitRaw(3, 0x0);
  alpha4Left.writeDigitRaw(0, 0xFFFF);
  alpha4Left.writeDisplay();
  delay(200);
  alpha4Left.writeDigitRaw(0, 0x0);
  alpha4Left.writeDigitRaw(1, 0xFFFF);
  alpha4Left.writeDisplay();
  delay(200);
  alpha4Left.writeDigitRaw(1, 0x0);
  alpha4Left.writeDigitRaw(2, 0xFFFF);
  alpha4Left.writeDisplay();
  delay(200);
  alpha4Left.writeDigitRaw(2, 0x0);
  alpha4Left.writeDigitRaw(3, 0xFFFF);
  alpha4Left.writeDisplay();
  delay(200);
  alpha4Left.clear();
  alpha4Left.writeDisplay();
  
  alpha4Right.writeDigitRaw(3, 0x0);
  alpha4Right.writeDigitRaw(0, 0xFFFF);
  alpha4Right.writeDisplay();
  delay(200);
  alpha4Right.writeDigitRaw(0, 0x0);
  alpha4Right.writeDigitRaw(1, 0xFFFF);
  alpha4Right.writeDisplay();
  delay(200);
  alpha4Right.writeDigitRaw(1, 0x0);
  alpha4Right.writeDigitRaw(2, 0xFFFF);
  alpha4Right.writeDisplay();
  delay(200);
  alpha4Right.writeDigitRaw(2, 0x0);
  alpha4Right.writeDigitRaw(3, 0xFFFF);
  alpha4Right.writeDisplay();
  delay(200);
  
  alpha4Right.clear();
  alpha4Right.writeDisplay();
  
/*  // display every character, 
  for (uint8_t i='!'; i<='z'; i++) {
    alpha4Left.writeDigitAscii(0, i);
    alpha4Left.writeDigitAscii(1, i+1);
    alpha4Left.writeDigitAscii(2, i+2);
    alpha4Left.writeDigitAscii(3, i+3);
    alpha4Left.writeDisplay();
    
    delay(300);
  }
*/
Serial.println("Start typing to display!");
}


char displaybuffer[8] = {' ', ' ', ' ', ' ',' ', ' ', ' ', ' '};

void loop() {
  while (! Serial.available()) return;
  
  char c = Serial.read();
  if (! isprint(c)) return; // only printable!
  
  // scroll down display
  displaybuffer[0] = displaybuffer[1];
  displaybuffer[1] = displaybuffer[2];
  displaybuffer[2] = displaybuffer[3];
  displaybuffer[3] = displaybuffer[4];
  displaybuffer[4] = displaybuffer[5];
  displaybuffer[5] = displaybuffer[6];
  displaybuffer[6] = displaybuffer[7];
  displaybuffer[7] = c;
 
  // set every digit to the buffer
  alpha4Left.writeDigitAscii(0, displaybuffer[0]);
  alpha4Left.writeDigitAscii(1, displaybuffer[1]);
  alpha4Left.writeDigitAscii(2, displaybuffer[2]);
  alpha4Left.writeDigitAscii(3, displaybuffer[3]);
  alpha4Right.writeDigitAscii(0, displaybuffer[4]);
  alpha4Right.writeDigitAscii(1, displaybuffer[5]);
  alpha4Right.writeDigitAscii(2, displaybuffer[6]);
  alpha4Right.writeDigitAscii(3, displaybuffer[7]);
 
  // write it out!
  alpha4Left.writeDisplay();
  alpha4Right.writeDisplay();
  delay(200);
}
