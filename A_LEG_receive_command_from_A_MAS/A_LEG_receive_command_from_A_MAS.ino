// NOTE: 1/9/17: This is old code, doesn't use FRAM and doesn't even use the Legacy command buffer.  Reference only.

// A-LEG main code rev: 07/11/16 by RDP.  Made getRS485Message() more powerful - does all error checking and only returns if valid record.
// This is just test code that receives an old-style RS485 message from A-MAS and sends it out to Legacy.  No FRAM, no tables, etc.

// Need to modify this code so that we check to see if the incoming serial
// buffer is non-zero, and if so, we call a function that will populate the
// buffer.  Then check the 2nd byte to see if it's for us, and either toss
// it out or act on it, depending.
// 8/18/15: Added 'T' for Boost (to turn on Legacy Powermasters.)
// 6/25/16: Boost command may not be needed; I think abs speed 0 and >0 turns Powermaster off and on
// I.e. P = PowerMaster on/off.  "P", Engine No., On|Off.  This is simply "Absolute Speed 1" for the appropriate "engine" number.
// POWERMASTER POWER ON: Legacy data : F8 B4 01 - ENG 90 Absolute speed 1
// POWERMASTER POWER OFF : Legacy data : F8 B4 00 - ENG 90 Absolute speed 0
/*
   Here is sample code that passes a byte array to a function, and gets
   the byte array back (because an array is automatically passed by reference.)

  void loop() {
	byte data[2];
	getdat(data);
  }

  void getdat(byte pdata[]) {
   pdata[0] = 'a';
   pdata[1] = 'b';
  }

*/

/* Revised 6/29/15 by RDP.

   So far, this code simply receives a command string from A_MAS via RS485, and
   sends it out serial port 3 to the Legacy base.

   Revised 9/2/15 to remove RS485 acknowledgment message, and don't bother check who it's from because it
   could only be from A-MAS.

*/

// Here are all the different Arduinos and their "addresses" (ID numbers) for communication.  6/25/16

#define ARDUINO_MAS     1       // Master Arduino (Main controller)
#define ARDUINO_LEG     2       // Output Legacy interface and accessory relay control
#define ARDUINO_SNS     3       // Input reads reads status of isolated track sections on layout
#define ARDUINO_BTN     4       // Input reads button presses by operator on control panel
#define ARDUINO_SWT     5       // Output throws turnout solenoids (Normal/Reverse) on layout
#define ARDUINO_LED     6       // Output controls the Green turnout indication LEDs on control panel
#define ARDUINO_OCC     7       // Output controls the Red/Green and White occupancy LEDs on control panel

// Here are constants related to the RS485 messages
#define RS485_MAX_LEN    20       // buffer length to hold the longest possible RS485 message
#define RS485_LEN_OFFSET  0       // first byte of message is always total message length in bytes
#define RS485_TO_OFFSET   1       // second byte of message is the ID of the Arduino the message is addressed to
#define RS485_FROM_OFFSET 2       // third byte of message is the ID of the Arduino the message is coming from
// Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
#define RS485_TX_CONTROL  4       // "transmit enable" output pin for MAX485 chip
#define RS485_TRANSMIT    HIGH    // how to set TX_CONTROL pin when we want to transmit RS485
#define RS485_RECEIVE     LOW     // how to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

// Wire.h is apparently needed for Digole serial for some reason
#include <Wire.h>

// The following lines are required by the Digole serial LCD display.
// The LCD display is connected to SERIAL #1.
#define _Digole_Serial_UART_  //To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp mydisp(&Serial1, 9600); //UART TX on arduino to RX on module

#define SPEAKER_PIN     2       // Piezo buzzer pull LOW to turn on
#define LED_PIN        13
// true = any non-zero number
// false = 0

// Define global bytes for each of the 9 possible used in Legacy commands.
// For the foreseeable future, we will *only* be using simple 3-byte Legacy commands.
// Nine-byte commands are for special effects, specifically voice from locos -- but we
// can manage most common messages via simpler 3-byte codes.

byte legacy11 = 0x00;   // F8 for Engine, or F9 for Train (or FE for emergency stop all)
byte legacy12 = 0x00;
byte legacy13 = 0x00;
byte legacy21 = 0xFB;   // Always FB for 2nd and 3rd "words", when used
byte legacy22 = 0x00;
byte legacy23 = 0x00;
byte legacy31 = 0xFB;  // Always FB for 2nd and 3rd "words", when used
byte legacy32 = 0x00;
byte legacy33 = 0x00;   // This will hold checksum for "multi-word" commands

// msgIncoming will store one incoming RS485 message.  Since we will be reading *every*
// message, most of them will probably not be for us, so we will just ignore those.
// However we need to define a byte array that is long enough to store the longest RS485
// message that any Arduino will send.  Start with a guess of 20 bytes.

byte msgIncoming[RS485_MAX_LEN];     // No need to initialize contents

void setup() {

	Serial.begin(115200);              // PC serial monitor window
	// Serial1 is for the Digole 20x4 LCD debug display, already set up
	Serial2.begin(9600);           // RS485 can run at 115200 baud
	Serial3.begin(9600);             // Legacy serial interface

	pinMode(RS485_TX_CONTROL, OUTPUT);    // Will get set LOW or HIGH depending on RS-485 read or write
	digitalWrite(RS485_TX_CONTROL, RS485_RECEIVE);  // Receive mode - any time we aren't transmitting
	pinMode(LED_PIN, OUTPUT);             // built-in LED
	digitalWrite(RS485_TX_CONTROL, LOW);  // make sure LED is turned off (OOPS 9/22/16 duplicates line two lines above, not a problem)
	pinMode(SPEAKER_PIN, OUTPUT);         // Piezo buzzer pin
	digitalWrite(SPEAKER_PIN, HIGH);       // make sure piezo speaker is off

	while (Serial2.read() > 0) {   // Clear out incoming RS485 serial buffer
		delay(1);
	}

	// The following commands are required by the Digole serial LCD display
	mydisp.begin();  // Required to initialize LCD
	mydisp.setLCDColRow(20, 4);  // Maps starting RAM address on LCD (if other than 1602)
	mydisp.disableCursor(); //We don't need to see a cursor on the LCD
	mydisp.backLightOn();
	delay(20); // About 15ms required to allow LCD to boot before clearing screen
	mydisp.clearScreen(); // FYI, won't execute as the *first* LCD command
	sendToLCD("A-LEG ready!");
}

void loop() {

	// If there is anything in the RS485 buffer, then get the message...
	// We care about messages address to us (from A-MAS), *and* addressed to A-MAS
	// from A-SNS, which tells us about sensors being tripped and cleared.
	// RS485 messages that we are about are:
	// A-MAS to A-LEG: Byte 4 = "R" new train and route command
	// A-SNS to A-MAS: Byte 4 = "C" sensor change: tripped or cleared
	// A-OCC to A-MAS: Byte 4 = "S" set smoke on or off (for all locos)

	if (Serial2.available()) {
		sendToLCD("Serial2 avail.");
		// msgIncoming is a byte array that is automatically passed by reference.
		getRS485Message(msgIncoming);
		if (msgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
			// The incoming message was for us!  For now we'll see it as a proof-of-concept command, eventually this will be a Route+Train command
			byte msgLen = msgIncoming[RS485_LEN_OFFSET];
			char legacyCmdType = msgIncoming[3];
			char legacyDevType = msgIncoming[4];
			byte legacyDevID = msgIncoming[5];
			byte legacyCmdIndex = msgIncoming[6];

			// Display the command received...

			Serial.println(legacyCmdType);
			Serial.println(legacyDevType);
			Serial.println(legacyDevID);
			Serial.println(legacyCmdIndex);
			Serial.println("");

			// Now, finally, we can process the message received from A_MAS!

			// legacyCmdType (1 char) [E|A|M|T|S|B|D|F|C]
			//   E = Emergency Stop All Devices (FE FF FF) (Note: all other parms in command are moot so ignore them)
			//   A = Absolute Speed command
			//   M = Momentum 0..7
			//   T = TMCC
			//   S = Stop Immediate (this engine/train only)
			//   B = Basic 3-byte Legacy command (always AAAA AAA1 XXXX XXXX)
			//   D = Railsounds Dialogue (9-byte Legacy extended command)
			//   F = Railsounds Effect (9-byte Legacy extended command)
			//   C = Effect Control (9-byte Legacy extended command)
			// legacyDevType (1 char) Engine or Train or blank if  n/a
			// legacyDevID (1 byte) Engine, Train, Acc'y, Switch, or Route number
			// legacyCmdIndex (1 byte) Hex value from Legacy protocol table, if applicable.
			//   N/a for CmdTypes E and S.

			switch (legacyCmdType) {
			case 'E':  // Emergency stop all devices
				Serial.println("Emergency Stop.");
				sendToLCD("Emergency Stop All.");
				legacy11 = 0xFE;
				legacy12 = 0xFF;
				legacy13 = 0xFF;
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
			case 'A':   // Set absolute speed
				Serial.println("Absolute speed command.");
				if (legacyDevType == 'E') {
					legacy11 = 0xF8;
					//         sendToLCD("Engine " + String(legacyDevID));
				}
				else {
					legacy11 = 0xF9;
					//         sendToLCD("Train " + String(legacyDevID));
				}
				legacy12 = legacyDevID * 2;  // Shift left one bit, fill with zero
				legacy13 = legacyCmdIndex;
				sendToLCD(legacyDevType + String(legacyDevID) + " A.S. " + String(legacyCmdIndex));
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
			case 'M':   // Set momentum
				Serial.println("Momentum.");
				if (legacyDevType == 'E') {
					legacy11 = 0xF8;
					//            sendToLCD("Engine " + String(legacyDevID));
				}
				else {
					legacy11 = 0xF9;
					//            sendToLCD("Train " + String(legacyDevID));
				}
				legacy12 = legacyDevID * 2;  // Shift left one bit, fill with zero
				legacy13 = 0xC8 + legacyCmdIndex;
				sendToLCD(legacyDevType + String(legacyDevID) + " Mom " + String(legacyCmdIndex));
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
      case 'T':  // TMCC command (only Action types supported here, and only Engines not Trains)
        Serial.println("TMCC Action.");
        sendToLCD("TMCC Action.");
        legacy11 = 0xFE;
        legacy12 = (legacyDevID >> 1);  // Could just do logical && with 11001000 to make it Train not Engine
        legacy13 = (legacyDevID << 7) + legacyCmdIndex;
				sendToLCD(legacyDevType + String(legacyDevID) + " Boost " + String(legacyCmdIndex));
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
			case 'S':    // Stop immediate (this engine or train)
				Serial.println("Stop immediate.");
				if (legacyDevType == 'E') {
					legacy11 = 0xF8;
					//            sendToLCD("Engine " + String(legacyDevID));
				}
				else {
					legacy11 = 0xF9;
					//            sendToLCD("Train " + String(legacyDevID));
				}
				legacy12 = legacyDevID * 2;  // Shift left one bit, fill with zero
				legacy13 = 0xFB;
				sendToLCD(legacyDevType + String(legacyDevID) + " Stop immed.");
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
			case 'B':
				Serial.println("Basic Legacy 3-byte command.");
				if (legacyDevType == 'E') {
					legacy11 = 0xF8;
					//            sendToLCD("Engine " + String(legacyDevID));
				}
				else {
					legacy11 = 0xF9;
					//            sendToLCD("Train " + String(legacyDevID));
				}
				legacy12 = (legacyDevID * 2) + 1;  // Shift left one bit, fill with '1'
				legacy13 = legacyCmdIndex;
				sendToLCD(legacyDevType + String(legacyDevID) + " Basic " + String(legacyCmdIndex));
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				break;
			case 'D':
			case 'F':
			case 'C':
				Serial.println("Railsounds D/F/C 9-byte command.");
				if (legacyDevType == 'E') {
					legacy11 = 0xF8;
					//            sendToLCD("Engine " + String(legacyDevID));
				}
				else {
					legacy11 = 0xF9;
					//            sendToLCD("Train " + String(legacyDevID));
				}
				legacy12 = (legacyDevID * 2) + 1;  // Shift left one bit, fill with '1'
				if (legacyCmdType == 'D') {
					legacy13 = 0x72;   // Railsounds Dialog Trigger
				}
				else if (legacyCmdType == 'F') {
					legacy13 = 0x74;   // Railsounds Effecs Triggers
				}
				else if (legacyCmdType == 'C') {
					legacy13 = 0x7C;   // Effects Controls
				}
				// Note: legacy21 and legacy31 are pre-defined; always "FB"
				legacy22 = (legacyDevID * 2);   // If it's an Engine, okay as-is
				if (legacyDevType == 'T') {     // If it's a Train, set bit to "1"
					legacy22 = legacy22 + 1;
				}
				legacy23 = legacyCmdIndex;
				legacy32 = legacy22;
				// Lionel Legacy command checksum.
				legacy33 = legacyChecksum(legacy12, legacy13, legacy22, legacy23, legacy32);
				sendToLCD(legacyDevType + String(legacyDevID) + " Ext Cmd " + String(legacyCmdIndex));
				Serial3.write(legacy11);
				Serial3.write(legacy12);
				Serial3.write(legacy13);
				Serial3.write(legacy21);
				Serial3.write(legacy22);
				Serial3.write(legacy23);
				Serial3.write(legacy31);
				Serial3.write(legacy32);
				Serial3.write(legacy33);
				break;
			}
			sendToLCD(String(legacy11, HEX) + " " + String(legacy12, HEX) + " " + String(legacy13, HEX));
		}
	}
}

////////////////////////////////
/// Define various functions ///
////////////////////////////////

void getRS485Message(byte tMsg[]) {

	// This function requires that there is at least one byte in the incoming serial
	// buffer, and it will wait until an entire incoming message is received, unless
	// it times out.
	// This only reads and returns one message at a time, regardless of how much more
	// data may be waiting in the serial buffer.
	// Input byte tmsg[] is the byte array that will hold the RS485 message.
	//   tmsg[] (filled) is also "returned" by the function since arrays are passed by reference.
	// Input byte whoAreWe is the constant representing which Arduino this is i.e. ARDUINO_LEG
	// Returns a byte status: 
	//   0 = AOK and for us
	//   1 = AOK but not for use
	//   2 = Buffer underflow - not enough chars to be a whole message
	//   3 = Buffer overflow - too many chars in Serial2()
	//   4 = Timeout waiting for bytes
	//   5 = Bad checksum

	byte tMsgLen = Serial2.peek();     // First byte will be message length
	if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
		sendToLCD("Underflow.");
		endWithFlashingLED(2);
	} 
	else {
		if (tMsgLen > 20) {            // Message too long to be any real message.  Fatal!
			sendToLCD("Overflow.");
			endWithFlashingLED(3);
		}
	}
	// We have a message of a valid length, though still don't know if the bytes are all there
	unsigned long tStartTime = millis();
	do {
		if (millis() > (tStartTime + 200)) {    // We'll wait up to 200ms (1/5 second) for the whole message to arrive before timeout
			sendToLCD("Serial timeout.");
			endWithFlashingLED(4);
		}
	} while (Serial2.available() < tMsgLen);  // Wait until all data in buffer
	for (int i = 0; i < tMsgLen; i++) {
		tMsg[i] = Serial2.read();
	}
	// calculate checksum
	if (tMsg[tMsgLen - 1] != calcChecksumCRC8(tMsg, tMsgLen - 1)) {   // Bad checksum.  Fatal!
		sendToLCD("Bad checksum.");
		endWithFlashingLED(5);
	}
	// At this point, we have a complete and legit message which may or may not be for us
}

byte calcChecksumCRC8(const byte *data, byte len) {
	// calcChecksumCRC8 returns the CRC-8 checksum to use or confirm.
	// Sample call: msgIncoming[msgLen - 1] = calcChecksumCRC8(msgIncoming, msgLen - 1);
	// We will send (sizeof(msgIncoming) - 1) and make the LAST byte
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

void endWithFlashingLED(int numFlashes) {
	// Flash the on-board LED a given number of times, forever.
	// Use this when program must terminate due to critical error.
	while (true) {
		for (int i = 1; i <= numFlashes; i++) {
			digitalWrite(LED_PIN, HIGH);
			chirp();  // sound a beep on the piezo
			delay(200);
			digitalWrite(LED_PIN, LOW);
			digitalWrite(SPEAKER_PIN, HIGH);
			delay(300);
		}
		delay(1000);
	}
}

void chirp() {
	digitalWrite(SPEAKER_PIN, LOW);  // turn on piezo
	delay(100);
	digitalWrite(SPEAKER_PIN, HIGH);
}

void sendToLCD(String s) {

	// The char arrays that hold the data are 21 bytes long, to allow for a
	// 20-byte text message (max.) plus the null character required.
	static char lineA[] = "                     ";
	static char lineB[] = "                     ";
	static char lineC[] = "                     ";
	static char lineD[] = "                     ";
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

byte legacyChecksum(byte leg12, byte leg13, byte leg22, byte leg23, byte leg32) {
	// Calculate the checksum required when sending "multi-word" commands to Legacy.
	// This function receives the five bytes from a 3-word Legacy command that are
	// used in calculation of the checksum, and returns the checksum.
	// Add up the five byte values, take the MOD256 remainder (which is automatically
	// done by virtue of storing the result of the addition in a byte-size variable,)
	// and then take the One's Complement which is simply inverting all of the bits.
	// We use the "~" operator to do a bitwise invert.

	return ~(leg12 + leg13 + leg22 + leg23 + leg32);
}

