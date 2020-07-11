/* Revised 7/10/20.
   This version sends a short message to each of the six slave Arduinos, one at a time, then repeats.

   Used with RS485_Test_B, this is a great way to confirm reliability of
   RS485 communication at various speeds.  Especially between A-MAS and A-LEG
   since they will be separated (A-LEG is not in the control panel) *and*
   once all 7 of the Arduinos are connected to the RS485 bus.
   We may need to add or remove bias and termination resistors to make
   this work.  But side-by-side, using just two Arduinos it works great!

   NOTE: I did notice that adding debug serial messages slows things down
   a lot!

   Test for Arduino A to send a command to Arduino B, wait for a response, and repeat many times to test reliability and speed.
   RESULTS: Sending and receiving 100 RS485 messages at the following baud rates:
   1200 baud: 13 seconds
   4800 baud: 3.5 seconds
   9600 baud: 1.8 seconds
  19200 baud: 1.0 seconds
 115200 baud: 0.3 seconds

   NEW RESULTS 4/22/16: Sending data to LCD outside of main loop.
   115200 baud, 100 iterations, elapsed time = 226 milliseconds (1/4 second.)
   1000 iterations = 2.264 seconds.
   1 iteration = .002264 seconds = 1/500th second per round trip.
   CONCLUSION: There is no way we will ever see an increase in performance if we add a second
   digital wire from A-MAS to A-SNS that indicates "okay to send," versus simply having A-MAS
   send an RS-485 message after it sees A-SNS pull the pin low.
   ONE DIGITAL SIGNAL WIRE BETWEEN A-SNS AND A-MAS IS ENOUGH; DO NOT NEED TWO!

   Messages use the following format:
    Byte 0: Number of data bytes.  Total packet length will be this number plus 4 (bytes 0..2, plus checksum.) I.e. 2 for 2 data bytes.
    Byte 1: Address of the Arduino the data is being sent to.
    Byte 2: Address of the Arduino the message is being sent from.
    Byte 3..n - 1: Data bytes.  Varies depending on who is sending what.
    Byte n: CRC8 checksum

  Note that the serial input buffer is only 128 brandyytes, which means that we need
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
#define ARDUINO_LED     6       // Output Arduino, illuminates green turnout position LEDs on control panel.
#define ARDUINO_OCC     7       // Output Arduino, illuminates red/blue block occupancy, and white occupancy LEDs on control panel.


// Wire.h is apparently needed for Digole serial for some reason
#include <Wire.h>

// The following lines are required by the Digole serial LCD display.
// The LCD display is connected to SERIAL #1.
#define _Digole_Serial_UART_  //To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp mydisp(&Serial1, 115200); //UART TX on arduino to RX on module.  9600 or 115200.

// The following constants hold true for ALL RS485 messages, regardless of
// which Arduino, and regardless of which one it is to or from:
#define RS485MSGLENOFFSET  0  // first byte is always length in bytes
#define RS485MSGTOOFFSET   1
#define RS485MSGFROMOFFSET 2
const byte RS485_TX_LED_PIN = 5;       // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
const byte RS485_RX_LED_PIN = 6;       // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data

// Miscellaneious constants for RS485 etc.
#define FALSE           0
#define TRUE            1

#define RS485TRANSMIT   HIGH
#define RS485RECEIVE    LOW

#define RS485TXENABLE   4       // Must set HIGH when in RS485 transmit mode, LOW when not transmitting
#define SPEAKERPIN 7  // Piezo buzzer connects here; pull LOW to turn on


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
  Serial2.begin (115200);  //  (57600);  // (115200);            // RS485  up to 115200

  pinMode (SPEAKERPIN, OUTPUT);      // Piezo buzzer pin
  digitalWrite(SPEAKERPIN, HIGH);
  pinMode (RS485TXENABLE, OUTPUT);   // driver output enable
  digitalWrite (RS485TXENABLE, RS485RECEIVE);  // receive mode = LOW, required to prevent RS485 chip from overheating.
  pinMode (RS485_TX_LED_PIN, OUTPUT);
  digitalWrite (RS485_TX_LED_PIN, LOW);    // Turn off RS485 "transmitting" LED
  pinMode (RS485_RX_LED_PIN, OUTPUT);
  digitalWrite (RS485_RX_LED_PIN, LOW);    // Turn off RS485 "receiving" LED
  pinMode (13, OUTPUT);              // On-board LED must be turned off manually on some boards
  digitalWrite (13, LOW);
  delay(1000); // See if we can eliminate simultaneous start-up error for receiving Arduino 7/10/20 *******************************
  while (Serial2.read() > 0) {  // Clear out incoming RS485 serial buffer
    Serial.println("Found a garbage byte in the RS485 buffer.");
    delay(1);
  }

  // The following commands are required by the Digole serial LCD display
  mydisp.begin();  // Required to initialize LCD
  mydisp.setLCDColRow(20, 4); // Maps starting RAM address on LCD (if other than 1602)
  mydisp.disableCursor(); //We don't need to see a cursor on the LCD
  mydisp.backLightOn();
  delay(25); // About 15ms required to allow LCD to boot before clearing screen *************** TRY CHANGING FROM 20 TO 25 TO SEE IF THIS HELPS LCD HANGING *************
  mydisp.clearScreen(); // FYI, won't execute as the *first* LCD command
  sendToLCD("RS485 XMIT.");

  delay(3000);   // We need this delay in order to give the slave a chance to get ready to receive data -- if they all turn on at once.
  // This may need to be addressed further when we get to the point of having multiple Arduinos communicating - they can't
  // just start sending RS485 data immediately -- there needs to be a delay so that all Arduinos are ready to read before
  // anyone starts transmitting.
}

void loop() {

  for (int i = 0; i < 6; i++) {    // i will represent each Arduino except A_MAS
    sendTheMessage(i);
  }
  Serial.println("End of loop.");
  sendToLCD("End of loop");

  delay(4000);
}

////////////////////////////////
/// Define various functions ///
////////////////////////////////

void sendTheMessage(int m) {
  digitalWrite(RS485_TX_LED_PIN, HIGH);  // Turn on blue transmit LED on RS485 board
  Serial.println("Assembling msg.");
  //sendToLCD("Assembling msg.");
  // Assemble RS485 command to A_LEG...
  byte msgLen = RS485MAXMSGLEN;   // Let's just try to make all messages a fixed length
  msgOutgoing[0] = msgLen;
  switch (m) {   // Set the message "TO" field
    case 0:
      msgOutgoing[1] = ARDUINO_LEG;
      sendToLCD("Message for A_LEG");
      break;
    case 1:
      msgOutgoing[1] = ARDUINO_SNS;
      sendToLCD("Message for A_SNS");
      break;
    case 2:
      msgOutgoing[1] = ARDUINO_BTN;
      sendToLCD("Message for A_BTN");
      break;
    case 3:
      msgOutgoing[1] = ARDUINO_SWT;
      sendToLCD("Message for A_SWT");
      break;
    case 4:
      msgOutgoing[1] = ARDUINO_OCC;
      sendToLCD("Message for A_OCC");
      break;
    case 5:
      msgOutgoing[1] = ARDUINO_LED;
      sendToLCD("Message for A_LED");
      break;
      //default:
  }
  msgOutgoing[2] = ARDUINO_MAS;  // From
  msgOutgoing[3] = random(256);
  msgOutgoing[4] = random(256);
  msgOutgoing[5] = random(256);
  msgOutgoing[6] = random(256);
  msgOutgoing[7] = calcChecksumCRC8(msgOutgoing, 7);  // Put CRC8 cksum for all but last byte into last byte
  Serial.print("Transmitting ");
  Serial.print(msgLen);
  Serial.println(" bytes.");
  digitalWrite(RS485TXENABLE, RS485TRANSMIT);  // transmit mode
  //  for (int i = 0; i < msgLen; i++) {
  //    Serial2.write(msgOutgoing[i]);
  //  }
  Serial2.write(msgOutgoing, msgLen);   // New technique, writes the whole buffer at once.  Just simpler.
  Serial2.flush();    // wait for transmission of outgoing serial data to complete
  digitalWrite(RS485TXENABLE, RS485RECEIVE);  // receive mode
  digitalWrite(RS485_TX_LED_PIN, LOW);  // Turn off blue transmit LED on RS485 board

  Serial.println("Message sent.");
  // sendToLCD("Message sent.");
  // Now wait for a response (will be almost idential message), check the checksum, and continue if no error.

  while (Serial2.available() == 0) {  // Wait for something to show up in the buffer
    delay(1);
  }
  Serial.println("Have reply.");
  // sendToLCD("Have reply.");
  getRS485Message(msgIncoming);  // Populate msgIncoming[] with the entire message
  Serial.println("Got it.");
  if (msgIncoming[RS485MSGTOOFFSET] != ARDUINO_MAS) {
    // If the message isn't for us, then our little test isn't working properly.
    sendToLCD("ERROR: Incoming not for A_MAS!");
    while (TRUE) {} // forever loop
  } else {   // The message was intended for us - so far, so good
    Serial.println("Verified to A-MAS.");
    byte msgLen = msgIncoming[RS485MSGLENOFFSET];
    // Now see if the CRC8 checksum is correct
    if (msgIncoming[msgLen - 1] != calcChecksumCRC8(msgIncoming, msgLen - 1)) {
      sendToLCD("Bad CRC.  Quit.");
      while (TRUE) {}
    }
    else {   // Incoming message had a good CRC - so far, so good again!
      Serial.println("Good CRC.");
      char c = msgIncoming[3];
      char d = msgIncoming[4];
      byte e = msgIncoming[5];
      byte f = msgIncoming[6];
    }
    switch (msgIncoming[RS485MSGFROMOFFSET]) {
    case ARDUINO_MAS:
      sendToLCD("ERR: Msg from A_MAS!");
      while (TRUE) {}  // exit in infinite loop
      break;
    case ARDUINO_LEG:
      sendToLCD("Message from A_LEG");
      break;
    case ARDUINO_SNS:
      sendToLCD("Message from A_SNS");
      break;
    case ARDUINO_BTN:
      sendToLCD("Message from A_BTN");
      break;
    case ARDUINO_SWT:
      sendToLCD("Message from A_SWT");
      break;
    case ARDUINO_OCC:
      sendToLCD("Message from A_OCC");
      break;
    case ARDUINO_LED:
      sendToLCD("Message from A_LED");
      break;
      //default:
    }

    for (int i = 3; i < 7; i++) {
      Serial.print("Message in [");
      Serial.print(i);
      Serial.print("] in = ");
      Serial.print(msgIncoming[i]);
      Serial.print(", out=");
      Serial.println(msgIncoming[i]);
      if (msgOutgoing[i] != msgIncoming[i]) {
        sendToLCD("Data mismatch.");
        while (TRUE) {}
      }
    }
    sendToLCD("Good receipt!");
  }
}

void getRS485Message(byte tmsg[]) {

  // This function requires that there is at least one byte in the incoming serial
  // buffer, and it will wait until an entire incoming message is received.
  // This only reads and returns one message at a time, regardless of how much more
  // data may be waiting in the serial buffer.
  // This *might* be a good place to confirm the number of available bytes to be
  // certain that the incoming serial buffer has not overflowed.

  sendToLCD("Getting data");
  byte msgLen = Serial2.peek();     // First byte will be message length
  sendToLCD("Waiting for msg");
  do {} while (Serial2.available() < msgLen);  // Wait until all data in buffer
  sendToLCD("Got msg");
  digitalWrite(RS485_RX_LED_PIN, HIGH);       // Turn on the receive LED
  for (int i = 0; i < msgLen; i++) {
    tmsg[i] = Serial2.read();
  }
  //delay(100); // so we can see the yellow LED
  digitalWrite(RS485_RX_LED_PIN, LOW);       // Turn off the receive LED
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
      mydisp.setPrintPos(0, 0);
      mydisp.print(lineB);
      mydisp.setPrintPos(0, 1);
      mydisp.print(lineC);
      mydisp.setPrintPos(0, 2);
      mydisp.print(lineD);
      mydisp.setPrintPos(0, 3);
      mydisp.print(lineA);
      break;
    case 1:
      s.toCharArray(lineB, 21);
      mydisp.setPrintPos(0, 0);
      mydisp.print(lineC);
      mydisp.setPrintPos(0, 1);
      mydisp.print(lineD);
      mydisp.setPrintPos(0, 2);
      mydisp.print(lineA);
      mydisp.setPrintPos(0, 3);
      mydisp.print(lineB);
      break;
    case 2:
      s.toCharArray(lineC, 21);
      mydisp.setPrintPos(0, 0);
      mydisp.print(lineD);
      mydisp.setPrintPos(0, 1);
      mydisp.print(lineA);
      mydisp.setPrintPos(0, 2);
      mydisp.print(lineB);
      mydisp.setPrintPos(0, 3);
      mydisp.print(lineC);
      break;
    case 3:
      s.toCharArray(lineD, 21);
      mydisp.setPrintPos(0, 0);
      mydisp.print(lineA);
      mydisp.setPrintPos(0, 1);
      mydisp.print(lineB);
      mydisp.setPrintPos(0, 2);
      mydisp.print(lineC);
      mydisp.setPrintPos(0, 3);
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
