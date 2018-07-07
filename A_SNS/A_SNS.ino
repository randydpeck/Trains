// A_SNS Rev: 10/01/17.
char APPVERSION[21] = "A-SNS Rev. 10/01/17";

// A-SNS simply attempts to send RS485 messages for, initially, every sensor that is tripped, and after that, for any change.
// This is an "OUTPUT-ONLY" module that does not monitor or respond to any other Arduino.
// A-SNS doesn't care about mode or anything else, except it waits for A-MAS gives us permission before sending each message.
// Even though we wait for A_MAS, we will never "lose" a sensor change unless it changes one way and then back again while
// A-MAS is not interested in asking, so who cares?
// A_SNS monitors occupany sensors via Centipede shift register.  There are 52 sensors as of 7/3/15, so we just need one
// Centipede with 64 bits of input.
// A-SNS sends messages for each individual sensor change, rather than simply dumping the latest 64-bit read from
// the sensor Centipede.  This means that A-MAS, A-LEG, and maybe other don't need to create a buffer of sensor
// changes, but simply handle each one as it arrives.
// Note that it is POSSIBLE that more than one sensor will be tripped by the time A-SNS is requested to send the
// sensor change(s) - due to a delay in A-MAS asking, more than one change could pile up.  A-SNS isn't keeping
// track of the order that sensor trips and clears take place, so it's POSSIBLE though very unlikely, that we could
// be sending sensor change out of order.
// It's even remotely possible, if A-MAS takes a long time to request a change that A-SNS has detected, that a train
// could have tripped and then cleared a sensor -- which would mean that A-MAS would miss that change.
// So A-MAS has to have a fast loop that queries for sensor changes faster than it is possible for a train to trip a
// sensor and then clear it.  Or faster than an arriving train might clear an entry sensor and then trip an exit
// (stopping) sensor.  This would always take at least a few seconds.
// All Arduinos will assume that initially, all sensors are unoccupied.  So when we start by sending sensor "trips"
// for all sensors that are occupied at the time the system is started, and thus everyone will know the status of all
// sensors as soon as the system starts up and A-MAS allows A-SNS to send the list of tripped sensors.
// To send a sensor change from A-SNS to A-MAS (and anyone else who is listening,) set a digital I/O pin (pin 8) LOW,
// which will be monitored by A-MAS.  A-MAS will send a "request for status" via RS485,at which point we will set
// the I/O pin back to HIGH, and transmit the sensor change (sensor number, tripped or cleared) via RS485.
// Why doesn't A-SNS just send an RS485 message to A-MAS?  Because A-MAS is the master, and no other Arduino is permitted
// to send any RS485 traffic "unsolicited."  Thus we avoid collisions, and A-MAS only has to deal with incoming data
// when it is ready to do so.  This applies to all other slave Arduinos as well as this one.
// We will also write messages to the LCD screen whenever we see a change in occupancy status.

// 09/16/17: Changed variable suffixes from No and Number to Num, Length to Len, Record to Rec, etc.
// 08/29/17: Cleaning up code, updated RS485 message protocol comments.  Changed sensor update array to structure.
// 01/20/17: Add a short chirp each time we detect a sensor trip (not clear), before we even send a record to A-MAS,
// so we can judge the delay until we hear a chirp from A-LEG indicating a commmand has been sent to Legacy.  Remove later.
// 11/02/16: initializeShiftRegisterPins() and delay in sendToLCD()
// 11/02/16: We get a pile-up of RS485 messages from A-SNS to A-MAS as soon as Manual mode is started, causing A-OCC
// (and others?) to have an overflowed incoming RS485 message - can't keep up.  One solution would be to have every
// Arduino queue all incoming RS485 traffic in a buffer like the turnout command buffer, but let's just try slowing
// A-SNS broadcasts down a bit, because I think this is the only time/place where we would get a flurry of RS485
// traffic.  During normal operation, it would be very unusual to get two occupancy changes within 100ms of each
// other, and even so, we would only be delaying the second trip message by at most 100ms.
// 11/02/16: Small tweak, to not display detected sensor change on LCD until the change has been requested by A-MAS.
// Previously, we might have a pile of changes to send, and only display the first one until A-MAS requests them.
// It was just a little odd -- why display just one?  We should either display them all, regardless of if/when A_MAS
// asks us for them, or display none until we are asked.  The latter was easy to implement so I went with that.
// 10/19/16: extended length of delay when checking if Halt pressed.

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-SNS.  Byte numbers represent offsets, so they start at zero. ***
// Because there are so many message types, we will document the protocols here and just use integers for offsets in the code.

// A-MAS to A-SNS:  Permission for A_SNS to send a sensor change record.
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  5
//      1  To         Byte  3 (A_SNS)
//      2  From       Byte  1 (A_MAS)
//      3  Command    Char  'S' Sensor status request (sent after it sees that a sensor changed, via a digital input from A-SNS.)
//      4  Checksum   Byte  0..255

// A-SNS to A-MAS:  Sensor status update for a single sensor change
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  7
//      1  To         Byte  1 (A_MAS)
//      2  From       Byte  3 (A_SNS)
//      3  Command    Char  'S' Sensor status update
//      4  Sensor #   Byte  1..52 (Note that as of Sept 2017, our code ignores sensor 53 and we have disconnected it from the layout)
//      5  Trip/Clr   Byte  [0|1] 0-Cleared, 1=Tripped
//      6  Checksum   Byte  0..255

// **************************************************************************************************************************

#include <Train_Consts_Global.h>
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?
byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
byte RS485MsgOutgoing[RS485_MAX_LEN];

char lcdString[LCD_WIDTH + 1];                   // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** SERIAL LCD DISPLAY: The following lines are required by the Digole serial LCD display, connected to serial port 1.
//const byte LCD_WIDTH = 20;        // Number of chars wide on the 20x04 LCD displays on the control panel
#define _Digole_Serial_UART_      // To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp LCDDisplay(&Serial1, 115200); //UART TX on arduino to RX on module
//char lcdString[LCD_WIDTH + 1];    // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** RS485 MESSAGES: Here are constants and arrays related to the RS485 messages
// Note that the serial input buffer is only 64 bytes, which means that we need to keep emptying it since there
// will be many commands between Arduinos, even though most may not be for THIS Arduino.  If the buffer overflows,
// then we will be totally screwed up (but it will be apparent in the checksum.)
// const byte RS485_MAX_LEN     = 20;    // buffer length to hold the longest possible RS485 message.  Just a guess.
//       byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
//       byte RS485MsgOutgoing[RS485_MAX_LEN];
// const byte RS485_LEN_OFFSET  =  0;    // first byte of message is always total message length in bytes
// const byte RS485_TO_OFFSET   =  1;    // second byte of message is the ID of the Arduino the message is addressed to
// const byte RS485_FROM_OFFSET =  2;    // third byte of message is the ID of the Arduino the message is coming from
// Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
// const byte RS485_TRANSMIT    = HIGH;  // HIGH = 0x1.  How to set TX_CONTROL pin when we want to transmit RS485
// const byte RS485_RECEIVE     = LOW;   // LOW = 0x0.  How to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

// *** SHIFT REGISTER: The following lines are required by the Centipede input/output shift registers.
#include <Wire.h>                 // Needed for Centipede shift register
#include <Centipede.h>
Centipede shiftRegister;          // create Centipede shift register object

// *** SENSOR STATE TABLE: Arrays contain 4 elements (unsigned ints) of 16 bits each = 64 bits = 1 Centipede
unsigned int sensorOldState[] = {65535,65535,65535,65535};
unsigned int sensorNewState[] = {65535,65535,65535,65535};

// *** SENSOR STATUS UPDATE TABLE: Store the sensor number and change type for an individual sensor change, to pass to functions etc.
struct sensorUpdateStruct {
  byte sensorNum;
  byte changeType;
};
sensorUpdateStruct sensorUpdate = {0, 0};

// *** MISC CONSTANTS AND GLOBALS: needed by A-SNS:
// true = any non-zero number
// false = 0

// We need a delay between sensor updates on RS485 so we don't overflow the incoming buffer of other Arduinos.
// A-OCC overflows if delay is 30ms or less, and it looks cool having it at the same 100ms delay as turnouts
// are thrown, so we'll use 100ms.
// This is relevant when we first start, sending sensor messages to A-MAS for all currently-occupied blocks.
const unsigned long SENSOR_DELAY_MS = 100;       // milliseconds.  Long since we compare to millis() which is long.
unsigned long sensorTimeUpdatedMS   = millis();  // So we don't send sensor updates more frequently than SENSOR_DELAY_MS.

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485  up to 115200
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // Set all Centipede shift register pins to INPUT for monitoring sensor trips
  initializePinIO();                    // Initialize all of the I/O pins and turn all LEDs on control panel off
  initializeLCDDisplay();               // Initialize the Digole 20 x 04 LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, just stop

  // We will not receive an RS485 query from A_MAS without us making a request to send, so we need to monitor for RS485
  // messages frequently, but toss them out unless we know we are waiting for one for us - which would not be here.
  // So dump however many messages might be in the incoming buffer before moving on...
  
  do { } while (RS485GetMessage(RS485MsgIncoming));  // Clear out incoming RS485 messages from serial input buffer

  // Read the status of the Centipede shift register and determine if there have been any new changes...
  // unsigned int sensorOldState[] = {65535,65535,65535,65535};  // Bit = 1 means sensor NOT tripped, so default all unoccupied.
  // unsigned int sensorNewState[] = {65535,65535,65535,65535};  // A bit changes to zero when it is occupied.
  // First time into loop, we will see bits set for all occupied sensors - probably several.
  // Use: shiftRegister.portRead([0...7]) - Reads 16-bit value from one port (chip)

  // Need to keep track of "before" and "after" values.
  bool stateChange = false;
  for (byte pinBank = 0; pinBank < 4; pinBank++) {  // This is for ONE Centipede shift register board, with four 16-bit chips.
    sensorNewState[pinBank] = shiftRegister.portRead(pinBank);  // Populate with 4 16-bit values
    if (sensorOldState[pinBank] != sensorNewState[pinBank]) {
      stateChange = true;
      // Figure which sensor...
      // Centipede input 0 = sensor #1.
      // changedBits uses Exclusive OR (^) to find changed bits in the current long int.
      // Note that if a Centipede bit is 1, it means the sensor is NOT tripped,
      // and if the Centipede bit is 0, it means the sensor has been tripped i.e. grounded.
      unsigned int changedBits = (sensorOldState[pinBank] ^ sensorNewState[pinBank]);
      for (byte pinBit = 0; pinBit < 16; pinBit++) {   // For each bit in this 16-bit integer (of 4)
        if ((bitRead(changedBits, pinBit)) == 1) {     // Found a bit that has changed, one way or the other - set or cleared
          byte sensorNum = ((pinBank * 16) + pinBit + 1);
          // The following is a bit counter-intuitive because Centipede bit will be opposite of how we set sensorUpdate[] bit.
          // We only do a test here (rather than do it in a single clever statement) so we can display English on the LCD.
          if (bitRead(sensorNewState[pinBank], pinBit) == 1) {   // Bit changed to 1 means it is clear (not grounded)
            sensorUpdate.changeType = 0;  // Cleared
            sprintf(lcdString, "Sensor %2d cleared.", sensorNum);
            chirp();  // Audible signal when sensor is cleared - single chirp
          } else {   // Bit changed to 0 means it was tripped (i.e. grounded)
            sensorUpdate.changeType = 1;  // Tripped
            sprintf(lcdString, "Sensor %2d tripped.", sensorNum);
            doubleChirp();  // Audible signal when sensor is tripped - double chirp
          }
          sensorUpdate.sensorNum = sensorNum;
          // We need a slight delay in the event that we have a flurry of sensor changes to transmit to A-MAS, to give the
          // other modules a chance to keep up.  No delay overflowed A-OCC input RS485 buffer when we started a mode.
          // Note that we impose a delay ONLY if we just already sent an update, normally there will be no delay.
          while ((millis() - sensorTimeUpdatedMS) < SENSOR_DELAY_MS) { }   // Wait a moment
          sensorTimeUpdatedMS = millis();    // Refresh the timer
          RS485fromSNStoMAS_SendSensorUpdate(sensorUpdate.sensorNum, sensorUpdate.changeType);  // Send this sensor change record to A-MAS
          sendToLCD(lcdString);            // Don't display a message on the LCD until after the change has been sent to A-MAS
          Serial.println(lcdString);
        }
      }
    }
  }

  // If there has been a change in sensor status, we will reset our old sensor states to reflect what was sent...
  // We do this independent of the above "check every sensor" loop because there could have been multiple sensor changes in that
  // loop, and we don't want to reset the states until all changes have been transmitted to A-MAS.
  if (stateChange) {
    // Now set "old" Centipede shift register value to the latest value, so only new changes will be seen...
    for (byte pinBank = 0; pinBank < 4; pinBank++) {
      sensorOldState[pinBank] = sensorNewState[pinBank];
    }
  }

}  // End of "loop()"

// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

void initializePinIO() {
  // Rev 7/14/16: Initialize all of the input and output pins  
  digitalWrite(PIN_SPEAKER, HIGH);  // Piezo
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_LED, HIGH);      // Built-in LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_REQ_TX_A_SNS_OUT, HIGH);
  pinMode(PIN_REQ_TX_A_SNS_OUT, OUTPUT);   // When a button has been pressed, tell A-MAS by pulling this pin LOW
  digitalWrite(PIN_HALT, HIGH);
  pinMode(PIN_HALT, INPUT);                  // HALT pin: monitor if gets pulled LOW it means someone tripped HALT.  Or change to output mode and pull LOW if we want to trip HALT.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
  return;
}

void initializeShiftRegisterPins() {
  // Rev: 10/26/16.  Set all Centipede shift register pins to input for monitoring button presses
  // We're only using one board, but it does no harm to initialize for two, in case we ever use another board.
  for (int i = 0; i < 8; i++) {         // For each of 4 chips per board / 2 boards = 8 chips @ 16 bits/chip
    shiftRegister.portMode(i, 0b1111111111111111);   // 0 = OUTPUT, 1 = INPUT.  Sets all pins on chip to INPUT
    shiftRegister.portPullup(i, 0b1111111111111111); // 0 = NO PULLUP, 1 = PULLUP.  Sets all pins on chip to PULLUP
  }
  return;
}

void RS485fromSNStoMAS_SendSensorUpdate(const byte tSensorNum, const byte tChangeType) {
  // Rev: 09/30/17.
  // Pull digital pin LOW to tell A-MAS we have a sensor change to report, please query us...
  digitalWrite (PIN_REQ_TX_A_SNS_OUT, LOW);
  // Wait for an RS485 command from A-MAS requesting sensor status.  Remember that it's possible that we may
  // receive some RS485 messages that are irrelevant first, so ignore all RS485 messages until we see one to A-SNS.
  // Also check if the Halt line has been pulled low, in case something crashed and message will never arrive.
  RS485MsgIncoming[RS485_TO_OFFSET] = ARDUINO_NUL;   // Anything other than ARDUINO_SNS
  do {
    checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, just stop
    RS485GetMessage(RS485MsgIncoming);
  } while (RS485MsgIncoming[RS485_TO_OFFSET] != ARDUINO_SNS);
  // Now we have an RS485 message to A-SNS.  Just for fun, let's confirm that it's from A_MAS, and that we
  // have the appropriate "request" command we are expecting...
  if ((RS485MsgIncoming[RS485_FROM_OFFSET] != ARDUINO_MAS) || (RS485MsgIncoming[3] != 'S')) {
    sprintf(lcdString, "Unexpected message!");
    sendToLCD(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }
  // Release the REQ_TX_A_SNS "I have a message for you, A-MAS" digital line back to HIGH state...
  digitalWrite (PIN_REQ_TX_A_SNS_OUT, HIGH);
  // Format and send the new status: Length, To, From, 'S', sensor number (byte), 0 if cleared or 1 if tripped, CRC
  RS485MsgOutgoing[RS485_LEN_OFFSET] = 7;  // Byte 0.  Length is 7 bytes.
  RS485MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_MAS;  // Byte 1.
  RS485MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_SNS;  // Byte 2.
  RS485MsgOutgoing[3] = 'S';   // 'S' for Sensor message.
  RS485MsgOutgoing[4] = tSensorNum;  // Sensor number 1..52
  RS485MsgOutgoing[5] = tChangeType;  // 0 if cleared, 1 if tripped
  RS485MsgOutgoing[6] = calcChecksumCRC8(RS485MsgOutgoing, 6);  // CRC checksum
  RS485SendMessage(RS485MsgOutgoing);
  return;
}

// ***************************************************************************
// *** HERE ARE FUNCTIONS USED BY VIRTUALLY ALL ARDUINOS *** REV: 09-12-16 ***
// ***************************************************************************

void RS485SendMessage(byte tMsg[]) {
  // 10/1/16: Updated from 9/12/16 to write entire message as single Serial2.write(msg,len) command.
  // This routine must *only* be called when an entire message is ready to write, not a byte at a time.
  digitalWrite(PIN_RS485_TX_LED, HIGH);       // Turn on the transmit LED
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_TRANSMIT);  // Turn on transmit mode
  Serial2.write(tMsg, tMsg[0]);  // tMsg[0] is always the number of bytes in the message, so write that many bytes
  // flush() makes it impossible to overflow the outgoing serial buffer, which CAN happen in my test code.
  // Although it is BLOCKING code, we'll use it at least for now.  Output buffer overflow is unpredictable without it.
  // Alternative would be to check available space in the outgoing serial buffer and stop on overflow, but how?
  Serial2.flush();    // wait for transmission of outgoing serial data to complete.  Takes about 0.1ms/byte.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // receive mode
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  return;
}

bool RS485GetMessage(byte tMsg[]) {
  // 10/19/16: Updated string handling for sendToLCD and Serial.print.
  // 10/1/16: Returns true or false, depending if a complete message was read.
  // If the whole message is not available, tMsg[] will not be affected, so ok to call any time.
  // Does not require any data in the incoming RS485 serial buffer when it is called.
  // Detects incoming serial buffer overflow and fatal errors.
  // If there is a fatal error, call endWithFlashingLED() which itself attempts to invoke an emergency stop.
  // This only reads and returns one complete message at a time, regardless of how much more data
  // may be waiting in the incoming RS485 serial buffer.
  // Input byte tmsg[] is the initialized incoming byte array whose contents may be filled.
  // tmsg[] is also "returned" by the function since arrays are passed by reference.
  // If this function returns true, then we are guaranteed to have a real/accurate message in the
  // buffer, including good CRC.  However, the function does not check if it is to "us" (this Arduino) or not.

  byte tMsgLen = Serial2.peek();     // First byte will be message length
  byte tBufLen = Serial2.available();  // How many bytes are waiting?  Size is 64.
  if (tBufLen >= tMsgLen) {  // We have at least enough bytes for a complete incoming message
    digitalWrite(PIN_RS485_RX_LED, HIGH);       // Turn on the receive LED
    if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too short!");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tMsgLen > RS485_MAX_LEN) {            // Message too long to be any real message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too long!");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tBufLen > 60) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 in buf ovrflw!");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      for (int b = 0; b < tBufLen; b++) {  // Display the contents of incoming RS485 buffer on serial display
        Serial.print(Serial2.read()); Serial.print(" ");
      }
      Serial.println();
      endWithFlashingLED(1);
    }
    for (int i = 0; i < tMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the tMsg[] byte array
      tMsg[i] = Serial2.read();
    }
    if (tMsg[tMsgLen - 1] != calcChecksumCRC8(tMsg, tMsgLen - 1)) {   // Bad checksum.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 bad checksum!");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // At this point, we have a complete and legit message with good CRC, which may or may not be for us.
    digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
    return true;
  } else {     // We don't yet have an entire message in the incoming RS485 bufffer
    return false;
  }
}

byte calcChecksumCRC8(const byte *data, byte len) {
  // Rev 6/26/16
  // calcChecksumCRC8 returns the CRC-8 checksum to use or confirm.
  // Used for RS485 messages
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
  // Rev 09/26/17 by RDP
  LCDDisplay.begin();                     // Required to initialize LCD
  LCDDisplay.setLCDColRow(LCD_WIDTH, 4);  // Maps starting RAM address on LCD (if other than 1602)
  LCDDisplay.disableCursor();             // We don't need to see a cursor on the LCD
  LCDDisplay.backLightOn();
  delay(20);                              // About 15ms required to allow LCD to boot before clearing screen
  LCDDisplay.clearScreen();               // FYI, won't execute as the *first* LCD command
  delay(100);                             // At 115200 baud, needs > 90ms after CLS before sending text.  No delay needed at 9600 baud.
  return;
}

void sendToLCD(const char nextLine[]) {
  // Display a line of information on the bottom line of the 2004 LCD display on the control panel, and scroll the old lines up.
  // INPUTS: nextLine[] is a char array, must be less than 20 chars plus null or system will trigger fatal error.
  // The char arrays that hold the data are LCD_WIDTH + 1 (21) bytes long, to allow for a
  // 20-byte text message (max.) plus the null character required.
  // Sample calls:
  //   char lcdString[LCD_WIDTH + 1];  // Global array to hold strings sent to Digole 2004 LCD
  //   sendToLCD("A-SWT Ready!");   Note: more than 20 characters will crash!
  //   sprintf(lcdString, "%.20s", "Hello world."); // 20 is hard-coded since don't know how to format with const LCD_WIDTH
  //   sendToLCD(lcdString);   i.e. "Hello world."
  //   int a = 7; unsigned long t = millis(); char c = 'R';
  //   sprintf(lcdString, "I %3i T %6lu C %3c", a, t, c);  Will also crash if longer than 20 chars!
  //   sendToLCD(lcdString);   i.e. "I...7.T...3149.C...R"
  // Rev 08/30/17 by RDP: Changed hard-coded "20"s to LCD_WIDTH
  // Rev 10/14/16 - TimMe and RDP

  // These static strings hold the current LCD display text.
  static char lineA[LCD_WIDTH + 1] = "                    ";  // Only 20 spaces because the compiler will
  static char lineB[LCD_WIDTH + 1] = "                    ";  // add a null at end for total 21 bytes.
  static char lineC[LCD_WIDTH + 1] = "                    ";
  static char lineD[LCD_WIDTH + 1] = "                    ";
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
  LCDDisplay.setPrintPos(0, 0);
  LCDDisplay.print(lineA);
  delay(1);
  LCDDisplay.setPrintPos(0, 1);
  LCDDisplay.print(lineB);
  delay(2);
  LCDDisplay.setPrintPos(0, 2);
  LCDDisplay.print(lineC);
  delay(3);
  LCDDisplay.setPrintPos(0, 3);
  LCDDisplay.print(lineD);
  delay(1);
  return;
}

void endWithFlashingLED(int numFlashes) {
  // Rev 10/05/16: Version for Arduinos WITHOUT relays that should be released.


  requestEmergencyStop();
  while (true) {
    for (int i = 1; i <= numFlashes; i++) {
      digitalWrite(PIN_LED, HIGH);
      chirp();
      digitalWrite(PIN_LED, LOW);
      delay(100);
    }
    delay(1000);
  }
  return;  // Will never get here due to above infinite loop
}

void chirp() {
  // Rev 10/05/16
  digitalWrite(PIN_SPEAKER, LOW);  // turn on piezo
  delay(10);
  digitalWrite(PIN_SPEAKER, HIGH);
  return;
}

void doubleChirp() {
  // Rev 09/30/17
  chirp();
  delay(10);
  chirp();
}

void requestEmergencyStop() {
  // Rev 10/05/16
  pinMode(PIN_HALT, OUTPUT);
  digitalWrite(PIN_HALT, LOW);   // Pulling this low tells all Arduinos to HALT (including A-LEG)
  delay(1000);
  digitalWrite(PIN_HALT, HIGH);  
  pinMode(PIN_HALT, INPUT);
  return;
}

void checkIfHaltPinPulledLow() {
  // Rev 10/29/16
  // Check to see if another Arduino, or Emergency Stop button, pulled the HALT pin low.
  // If we get a false HALT due to turnout solenoid EMF, it only lasts for about 8 microseconds.
  // So we'll sit in a loop and check twice to confirm if it's a real halt or not.
  // Updated 10/25/16 to increase EMF spike delay from 25 microseconds to 50 microseconds, because
  // A-MAS was tripped once, presumably by a "double" spike.
  // Rev 10/19/16 extend delay from 50 microseconds to 1 millisecond to try to eliminate false trips.
  // No big deal because this only happens very rarely.
  if (digitalRead(PIN_HALT) == LOW) {   // See if it lasts a while
    delay(1);  // Pause to let a spike resolve
    if (digitalRead(PIN_HALT) == LOW) {  // If still low, we have a legit Halt, not a neg voltage spike
      chirp();
      sprintf(lcdString, "%.20s", "HALT pin low!  End.");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    } else {
      sprintf(lcdString,"False HALT detected.");
      sendToLCD(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}
