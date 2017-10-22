/* Revised 3/17/16.
   
   Test for Arduino A to send a command to Arduino B, wait for a response, and repeat many times to test reliability and speed.

   Messages use the following format:
    Byte 0: Number of data bytes.  Total packet length will be this number plus 4 (bytes 0..2, plus checksum.) I.e. 2 for 2 data bytes.
    Byte 1: Address of the Arduino the data is being sent to.
    Byte 2: Address of the Arduino the message is being sent from.
    Byte 3..n - 1: Data bytes.  Varies depending on who is sending what.
    Byte n: CRC8 checksum

  Note that the serial input buffer is only 128 bytes, which means that we need
  to keep emptying it since there will be many commands between Arduinos, even
  though most may not be for THIS Arduino.  If the buffer overflows, then we
  will be totally screwed up (but it will be apparent in the checksum.)

*/

// Here are all the different Arduinos and their "addresses" (ID numbers) for communication.

#define ARDUINO_MAS     1       // Master Arduino ID (Main controller)
#define ARDUINO_LEG     2       // Legacy Interface and Accessory Control Arduino (talks to Lionel Legacy CAB-2)
#define ARDUINO_SNS     3       // Monitors actual track occupancy via isolated track sections on layout (via time-delay relays.)
#define ARDUINO_BTN     4       // Monitors operator pressing the turnout buttons on the control panel.
#define ARDUINO_SWT     5       // Turnout Solenoid Controller Arduino (throws turnouts)
#define ARDUINO_OCC     6       // Output Arduino, illuminates red/blue block occupancy, and white occupancy LEDs on control panel.
#define ARDUINO_LED     7       // Output Arduino, illuminates green turnout position LEDs on control panel.

// Wire.h is apparently needed for Digole serial for some reason
#include <Wire.h>

// The following lines are required by the Digole serial LCD display.
// The LCD display is connected to SERIAL #1.
#define _Digole_Serial_UART_  //To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp mydisp(&Serial1, 115200); //UART TX on arduino to RX on module
char lcdString[21];  // Global array to hold strings sent to Digole 2004 LCD

// The following constants hold true for ALL RS485 messages, regardless of
// which Arduino, and regardless of which one it is to or from:
#define RS485MSGLENOFFSET  0  // first byte is always length in bytes
#define RS485MSGTOOFFSET   1
#define RS485MSGFROMOFFSET 2

// Miscellaneious constants for RS485 etc.
#define FALSE           0
#define TRUE            1

#define RS485TRANSMIT   HIGH
#define RS485RECEIVE    LOW

#define RS485TXENABLE   4       // Must set HIGH when in RS485 transmit mode, LOW when not transmitting
#define SPEAKERPIN      26       // Piezo buzzer connects positive here
const byte HALT_PIN   =  9;      // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF

// msgIncoming will store one incoming RS485 message.
// We need to define a byte array that is long enough to store the longest RS485
// message that any Arduino will send.  Start with a guess of 20 bytes.

#define RS485MAXMSGLEN 20    // We're going to try to make all messages the same length
  // Max length of 7 (8 bytes) allows for 4 bytes of data, plus Length (maybe redundant but a good check), To, From, and CRC
byte msgIncoming[RS485MAXMSGLEN];     // No need to initialize contents
byte msgOutgoing[RS485MAXMSGLEN];

byte randomNumber;  // Just for our proof of concept, to hold a message byte

// *********************** RS485 BUFFER CODE *************************************

// NOTE: ONLY need the following if we create an incoming RS485 message buffer.  But here, we will more likely use a turnout
// command buffer, which does not always have a 1:1 correspondence with RS485 commands because they could be "set route"
// commands in addition to "set turnout" commands.  Anyway we might not even need the RS485 incoming buffer...
// IMPORTANT!  9/14/16: We set the max buffer size to 4 elements in order to INTENTIONALLY try to overflow the buffer
// to confirm that the overflow logic will work.
const byte RS485_MAX_BUFFER = 12;     // Max number of RS485 incoming bytes that might pile up in the buffer before can be processed.

// Create a circular buffer to store incoming RS485 commands, that are relevant, in case we can't keep up...
byte RS485InCommandHead = 0;     // First element in the circular RS485 incoming buffer array
byte RS485InCommandLength = 0;   // Number of elements active
byte RS485InCommandBuffer[RS485_MAX_BUFFER];  // This many bytes in the circular incoming RS485 command buffer

// ********************* RS485 BUFFER CODE END ************************************

void setup() {
// Might need Wire.begin(); for Digole serial LCD but apparently not
  Serial.begin(115200);                // PC serial monitor window
  // Serial1 has already been set up (above) for the Digole 20x4 LCD
  Serial2.begin (115200);  // (57600); // (115200);            // RS485  up to 115200

  pinMode (SPEAKERPIN, OUTPUT);      // Piezo buzzer pin
  digitalWrite (SPEAKERPIN, HIGH);
  pinMode (RS485TXENABLE, OUTPUT);   // driver output enable
  digitalWrite (RS485TXENABLE, RS485RECEIVE);  // receive mode
  digitalWrite(HALT_PIN, HIGH);
  pinMode(HALT_PIN, INPUT);  


  initializeLCDDisplay();
  
  delay(300);
  while(Serial2.read() > 0) {   // Clear out incoming RS485 serial buffer
   delay(1);
  }  
  Serial.println("Checking incoming serial buffer...");
  sprintf(lcdString, "%.20s", "Waiting");
  sendToLCD(lcdString);  
}

void loop() {

  // Check if HALT pin pulled LOW
  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop
/*
  if (Serial2.available()) {
    Serial.println("Serial received...");
    sprintf(lcdString, "%.20s", "Serial rec'd");
    sendToLCD(lcdString);

    getRS485Message(msgIncoming);  // Populate msgIncoming[] with the entire message    
    sprintf(lcdString, "%.20s", "So far so good.");
    sendToLCD(lcdString);
    byte msgLen = msgIncoming[RS485MSGLENOFFSET];
    // Now see if the CRC8 checksum is correct
    if (msgIncoming[msgLen - 1] != calcChecksumCRC8(msgIncoming, msgLen - 1)) {
       sprintf(lcdString, "%.20s", "Bad CRC.  Quit.");
       sendToLCD(lcdString);
       while(TRUE) { }
    } else {   // Incoming message had a good CRC - so far, so good again!
      sprintf(lcdString, "%.20s", "Good msg rec'd.");
      sendToLCD(lcdString);
    }

    Serial.println("End of loop.");
  }
*/
}


////////////////////////////////
/// Define various functions ///
////////////////////////////////

void getRS485Message(byte tmsg[]) {
  
  // This function requires that there is at least one byte in the incoming serial
  // buffer, and it will wait until an entire incoming message is received.
  // This only reads and returns one message at a time, regardless of how much more
  // data may be waiting in the serial buffer.
  // This *might* be a good place to confirm the number of available bytes to be
  // certain that the incoming serial buffer has not overflowed.
//Serial.println("Turning on yellow LED...");

  byte msgLen = Serial2.peek();     // First byte will be message length
Serial.print("Message length is ");
Serial.println(msgLen);
Serial.println(String(msgLen));
sprintf(lcdString, "Message len %2i", msgLen);
sendToLCD(lcdString);
Serial.println(lcdString);
Serial.print("Getting bytes...");
  do {} while (Serial2.available() < msgLen);  // Wait until all data in buffer
  for (int i = 0; i < msgLen; i++) {  

    tmsg[i] = Serial2.read();
Serial.print(String(tmsg[i]));
Serial.print(" ");
Serial.println(tmsg[i]);
  }
Serial.println("...and done.");


}

byte calcChecksumCRC8(const byte *data, byte len) {
  // calcChecksumCRC8 returns the CRC-8 checksum to use or confirm.
  // Sample call: msg[msgLen - 1] = calcChecksumCRC8(msg, msgLen - 1);
  // We will send (sizeof(msg) - 1) and make the LAST byte
  // the CRC byte - so not calculated as part of itself ;-)
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}

void initializeLCDDisplay() {
  // Rev 10/14/16: Initialize the Digole 20 x 04 LCD display.  Added delay to work at 115200 baud.
  mydisp.begin();              // Required to initialize LCD
  mydisp.setLCDColRow(20, 4);  // Maps starting RAM address on LCD (if other than 1602)
  mydisp.disableCursor();      // We don't need to see a cursor on the LCD
  mydisp.backLightOn();
  delay(20);                   // About 15ms required to allow LCD to boot before clearing screen
  mydisp.clearScreen();        // FYI, won't execute as the *first* LCD command
  delay(100);                  // At 115200 baud, needs > 90ms after CLS before sending text.  No delay needed at 9600 baud.
  sprintf(lcdString, "%.20s", "A-XXX Ready!"); 
  sendToLCD(lcdString);
  Serial.println(lcdString);
}

void sendToLCD(const char *nextLine)  {
  // Rev 10/14/16 - TimMe and RDP
  // The char arrays that hold the data are 21 bytes long, to allow for a
  // 20-byte text message (max.) plus the null character required.
  // Sample calls:
  //   char lcdString[21];  // Global array to hold strings sent to Digole 2004 LCD
  //   sendToLCD("A-SWT Ready!");   Note: more than 20 characters will crash!
  //   sprintf(lcdString, "%.20s", "Hello world."); 
  //   sendToLCD(lcdString);   i.e. "Hello world."
  //   int a = 7; unsigned long t = millis(); char c = 'R';
  //   sprintf(lcdString, "I %3i T %6lu C %3c", a, t, c);  Will also crash if longer than 20 chars!
  //   sendToLCD(lcdString);   i.e. "I...7.T...3149.C...R"

  // These static strings hold the current LCD display text.
  static char lineA[] = "                    ";  // Only 20 spaces because the compiler will
  static char lineB[] = "                    ";  // add a null at end for total 21 bytes.
  static char lineC[] = "                    ";
  static char lineD[] = "                    ";

  if ((nextLine == (char *)NULL) || (strlen(nextLine) > 20)) endWithFlashingLED(13);

  // Scroll all lines up to make room for the new bottom line.
  strcpy(lineA, lineB);
  strcpy(lineB, lineC);
  strcpy(lineC, lineD);
  strncpy(lineD, nextLine, 20);         // Copy the new bottom line into lineD, padded to 20 chars with nulls.

  int newLineLength = strlen(lineD);    // Get the length of the new bottom line (to the first null char.)

  // Pad the new bottom line with trailing spaces as needed.
  while(newLineLength < 20) lineD[newLineLength++] = ' ';  // Last byte not touched; always remains "null."

  // Update the display.
  mydisp.setPrintPos(0, 0);
  mydisp.print(lineA);
  mydisp.setPrintPos(0, 1);
  mydisp.print(lineB);
  mydisp.setPrintPos(0, 2);
  mydisp.print(lineC);
  mydisp.setPrintPos(0, 3);
  mydisp.print(lineD);
 
  return;
}

void beep() {
  digitalWrite(SPEAKERPIN, LOW);
  delay(100);
  digitalWrite(SPEAKERPIN, HIGH);
}

void endWithFlashingLED(int numFlashes) {
  while (true) {
    for (int i = 1; i <= numFlashes; i++) {
      beep();
      delay(100);
    }
    delay(1000);
  }
}

void checkIfHaltPinPulledLow() {
  // Rev 10/22/16
  // Check to see if another Arduino, or Emergency Stop button, pulled the HALT pin low.
  // If we get a false HALT due to turnout solenoid EMF, it only lasts for about 8 microseconds.
  // So we'll sit in a loop and check twice to confirm if it's a real halt or not.
  if (digitalRead(HALT_PIN) == LOW) {   // See if it lasts more than about 16 microseconds
    delay(1);  // Much longer than necessary
    if (digitalRead(HALT_PIN) == LOW) {  // If still low, we have a legit Halt, not a neg voltage spike
      sprintf(lcdString, "%.20s", "HALT pin low!  End.");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    }
  }
}

