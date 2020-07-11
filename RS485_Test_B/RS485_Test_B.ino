/* Revised 7/10/20.
   
   Test for master Arduino to send a command to each slave Arduino, wait for a response, and repeat many times to test reliability and speed.
   THIS IS THE CODE FOR THE SLAVES.

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
DigoleSerialDisp mydisp(&Serial1, 9600); //UART TX on arduino to RX on module

// The following constants hold true for ALL RS485 messages, regardless of
// which Arduino, and regardless of which one it is to or from:
#define RS485MSGLENOFFSET  0  // first byte is always length in bytes
#define RS485MSGTOOFFSET   1
#define RS485MSGFROMOFFSET 2
const byte RS485_TX_LED_PIN        =  5;       // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
const byte RS485_RX_LED_PIN        =  6;       // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
const byte LED_PIN                 = 13;       // Built-in LED always pin 13
const byte REQ_A_LEG_HALT_PIN      =  9;       // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF
const byte SPEAKER_PIN             =  7;       // Output: Piezo buzzer connects positive here

// Miscellaneious constants for RS485 etc.
#define FALSE           0
#define TRUE            1

#define RS485TRANSMIT   HIGH
#define RS485RECEIVE    LOW

#define RS485TXENABLE   4       // Must set HIGH when in RS485 transmit mode, LOW when not transmitting
#define SPEAKERPIN      26       // Piezo buzzer connects positive here

// msgIncoming will store one incoming RS485 message.
// We need to define a byte array that is long enough to store the longest RS485
// message that any Arduino will send.  Start with a guess of 20 bytes.

#define RS485MAXMSGLEN 8    // We're going to try to make all messages the same length
  // Max length of 7 (8 bytes) allows for 4 bytes of data, plus Length (maybe redundant but a good check), To, From, and CRC
byte msgIncoming[RS485MAXMSGLEN];     // No need to initialize contents
byte msgOutgoing[RS485MAXMSGLEN];

byte randomNumber;  // Just for our proof of concept, to hold a message byte

void setup() {
// Might need Wire.begin(); for Digole serial LCD but apparently not
  Serial.begin(115200);                // PC serial monitor window
  // Serial1 has already been set up (above) for the Digole 20x4 LCD
  Serial2.begin (115200);  // (57600); // (115200);            // RS485  up to 115200

  pinMode (SPEAKERPIN, OUTPUT);      // Piezo buzzer pin
  digitalWrite (SPEAKERPIN, HIGH);
  pinMode (RS485TXENABLE, OUTPUT);   // driver output enable
  digitalWrite (RS485TXENABLE, RS485RECEIVE);  // receive mode
  pinMode(RS485_TX_LED_PIN, OUTPUT);
  digitalWrite(RS485_TX_LED_PIN, LOW);    // Turn off RS485 "transmitting" LED
  pinMode(RS485_RX_LED_PIN, OUTPUT);
  digitalWrite(RS485_RX_LED_PIN, LOW);    // Turn off RS485 "receiving" LED

  // The following commands are required by the Digole serial LCD display
  mydisp.begin();  // Required to initialize LCD
  mydisp.setLCDColRow(20,4);  // Maps starting RAM address on LCD (if other than 1602)
  mydisp.disableCursor(); //We don't need to see a cursor on the LCD
  mydisp.backLightOn();
  delay(25); // About 15ms required to allow LCD to boot before clearing screen. 7/11/20 changed from 20 to 25 to see if it fixes LCD problems.
  mydisp.clearScreen(); // FYI, won't execute as the *first* LCD command
  sendToLCD("RS485 Receive");
  delay(1000);
  while(Serial2.read() > 0) {   // Clear out incoming RS485 serial buffer
   delay(1);
  }  
  // delay(1000); // Added to just wait, but not so long that we miss the first broadcast from A_MAS
}

void loop() {

  // Wait for incoming message, check the checksum, and send back if no error.
  Serial.println("Checking incoming serial buffer...");
  //sendToLCD("Waiting...");
  while(Serial2.available() == 0) {   // Wait for something to show up in the buffer
    delay(1);
  }
  Serial.println("Serial received...");
  //sendToLCD("Serial received.");
  // Don't bother getting the message until we have the whole thing in the RS485 serial buffer
  byte msgLen = Serial2.peek();     // First byte will be message length
  Serial.print("Message length is ");
  Serial.println(msgLen);
  Serial.println(String(msgLen));
  //sendToLCD("Msglen " + String(msgLen));
  if (getRS485Message(msgIncoming)) {  // Returns true only if a complete message was received via RS485
    Serial.println("We have the whole message!  Process it...");
    switch (msgIncoming[RS485MSGTOOFFSET]) {
      case ARDUINO_MAS:
        sendToLCD("Message for A_MAS");
        break;
      case ARDUINO_LEG:
        sendToLCD("Message for A_LEG");
        break;
      case ARDUINO_SNS:
        sendToLCD("Message for A_SNS");
        break;
      case ARDUINO_BTN:
        sendToLCD("Message for A_BTN");
        break;
      case ARDUINO_SWT:
        sendToLCD("Message for A_SWT");
        break;
      case ARDUINO_OCC:
        sendToLCD("Message for A_OCC");
        break;
      case ARDUINO_LED:
        sendToLCD("Message for A_LED");
        break;
      //default:
    }
    // Now if the message was for us, send a reply...
    if (msgIncoming[RS485MSGTOOFFSET] == ARDUINO_OCC) {  // LEG, SNS, BTN, SWT, OCC, or LED  // ******* CHANGE 1 of 2 HERE
      byte msgLen = msgIncoming[RS485MSGLENOFFSET];
      // Now see if the CRC8 checksum is correct
      if (msgIncoming[msgLen - 1] != calcChecksumCRC8(msgIncoming, msgLen - 1)) {
        sendToLCD("Bad CRC.  Quit.");
        while (TRUE) {}
      }
      else {   // Incoming message had a good CRC - so far, so good again!
        digitalWrite(RS485_TX_LED_PIN, HIGH);  // Turn on blue transmit LED on RS485 board
        Serial.println("CRC was good.");
        //sendToLCD("Good msg rec'd.");
        byte msgLen = 8;
        msgOutgoing[0] = msgLen;
        msgOutgoing[1] = ARDUINO_MAS;  // To
        msgOutgoing[2] = ARDUINO_OCC;  // From                                                  ******** CHANGE 2 of 2 HERE
        msgOutgoing[3] = msgIncoming[3];
        msgOutgoing[4] = msgIncoming[4];
        msgOutgoing[5] = msgIncoming[5];
        msgOutgoing[6] = msgIncoming[6];
        msgOutgoing[7] = calcChecksumCRC8(msgOutgoing, 7);  // Put CRC8 cksum for all but last byte into last byte
        Serial.print("Transmitting ");
        Serial.print(msgLen);
        Serial.println(" bytes.");
        digitalWrite(RS485TXENABLE, RS485TRANSMIT);  // transmit mode
        Serial2.write(msgOutgoing, msgLen);   // New technique, writes the whole buffer at once.  Just simpler.
        //for (int i = 0; i < msgLen; i++) {
          //      Serial2.write(msgOutgoing[i]);
        //}
        Serial2.flush();    // wait for transmission of outgoing serial data to complete
        digitalWrite(RS485TXENABLE, RS485RECEIVE);  // receive mode
        digitalWrite(RS485_TX_LED_PIN, LOW);  // Turn off blue transmit LED on RS485 board
        Serial.println("Reply sent to A_MAS.");
        sendToLCD("Reply sent to A_MAS!");
      }  // End of "we have enough RS485 bytes for a complete message, so process it
    }
  }
  // Serial.println("End of loop.");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////
/// Define various functions ///
////////////////////////////////

boolean getRS485Message(byte tMsg[]) {
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
    digitalWrite(RS485_RX_LED_PIN, HIGH);       // Turn on the receive LED
    if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sendToLCD(F("Underflow."));
      endWithFlashingLED(1);
    } else if (tMsgLen > 20) {            // Message too long to be any real message.  Fatal!
      sendToLCD(F("Overflow."));
      endWithFlashingLED(1);
    } else if (tBufLen > 54) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sendToLCD(F("RS485 in buf ovflow."));
      endWithFlashingLED(1);
    }
    for (int i = 0; i < tMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the tMsg[] byte array
      tMsg[i] = Serial2.read();
    }
    if (tMsg[tMsgLen - 1] != calcChecksumCRC8(tMsg, tMsgLen - 1)) {   // Bad checksum.  Fatal!
        sendToLCD(F("Bad checksum."));
        endWithFlashingLED(1);
    }
    // At this point, we have a complete and legit message with good CRC, which may or may not be for us.
delay(100); // so we can see the yellow LED
    digitalWrite(RS485_RX_LED_PIN, LOW);       // Turn off the receive LED
    return true;
  } else {     // We don't yet have an entire message in the incoming RS485 bufffer
    return false;
  }
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


void sendToLCD(String s) {
  
  // The char arrays that hold the data are 21 bytes long, to allow for a
  // 20-byte text message (max.) plus the null character required.
  static char lineA[ ] = "                     ";
  static char lineB[ ] = "                     ";
  static char lineC[ ] = "                     ";
  static char lineD[ ] = "                     ";
  // Top line is lost each time this routine is called, bottom 3 lines scroll up.
  static int topLine = 0;  // 0..3 pointer changes each time we print to display

  mydisp.clearScreen();
  switch (topLine) {
    case 0:
      s.toCharArray(lineA, 21);
      mydisp.setPrintPos(0,0);
      mydisp.print(lineB);
      mydisp.setPrintPos(0,1);
      mydisp.print(lineC);
      mydisp.setPrintPos(0,2);
      mydisp.print(lineD);
      mydisp.setPrintPos(0,3);
      mydisp.print(lineA);
      break;
    case 1:
      s.toCharArray(lineB, 21);
      mydisp.setPrintPos(0,0);
      mydisp.print(lineC);
      mydisp.setPrintPos(0,1);
      mydisp.print(lineD);
      mydisp.setPrintPos(0,2);
      mydisp.print(lineA);
      mydisp.setPrintPos(0,3);
      mydisp.print(lineB);
      break;
     case 2:
      s.toCharArray(lineC, 21);
      mydisp.setPrintPos(0,0);
      mydisp.print(lineD);
      mydisp.setPrintPos(0,1);
      mydisp.print(lineA);
      mydisp.setPrintPos(0,2);
      mydisp.print(lineB);
      mydisp.setPrintPos(0,3);
      mydisp.print(lineC);
      break;     
     case 3:
      s.toCharArray(lineD, 21);
      mydisp.setPrintPos(0,0);
      mydisp.print(lineA);
      mydisp.setPrintPos(0,1);
      mydisp.print(lineB);
      mydisp.setPrintPos(0,2);
      mydisp.print(lineC);
      mydisp.setPrintPos(0,3);
      mydisp.print(lineD);
      break;    
  }
  topLine = (topLine + 1) % 4;

}

void beep() {
  digitalWrite(SPEAKERPIN, LOW);
  delay(100);
  digitalWrite(SPEAKERPIN, HIGH);
}

void endWithFlashingLED(int numFlashes) {
  // Updated 10/1/16 to call chirp()
  // Updated 7/15/16 from 7/7/16 to add "requestEmergencyStop"
  // since we only come here when fatal error occurs.
  // Flash the on-board LED a given number of times, forever, and chirp.
  requestEmergencyStop();
  while (true) {
    for (int i = 1; i <= numFlashes; i++) {
      digitalWrite(LED_PIN, HIGH);
      chirp();
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
    delay(800);
  }
}

void chirp() {
  // Rev 6/26/16
  digitalWrite(SPEAKER_PIN, LOW);  // turn on piezo
  delay(100);
  digitalWrite(SPEAKER_PIN, HIGH);
}

void requestEmergencyStop() {
  pinMode(REQ_A_LEG_HALT_PIN, OUTPUT);
  digitalWrite(REQ_A_LEG_HALT_PIN, LOW);   // Pulling this low tells all Arduinos to HALT (including A-LEG)
}

