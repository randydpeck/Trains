// NOTE 1/9/17: This is newer than A_LEG_receive_command_from_A_MAS, but still this only populates FRAM with test data
// and executes.  It contains code that we will be using, but is probably NOT a good starting point for the "real" A_LEG.
// Instead, start with A_MAS, which uses the most current version of LCD code, getRS485Message, consts, etc.

// A-LEG FRAM test rev: 07/07/16 by RDP
// Updated 7/6/16 to accept TMCC commands, specifically for Stationsounds Diner cars and other TMCC devices
// Also REMOVED the previous 'T' command which was "boosT" which we don't need.

/*
Device Type(1 char) (Engine, Train, Accessory, Switch, Route [E|T|A|S|R]
Device Number (1 byte) (engine, train, accessory, switch, or route number)[0..99]
	NOTE: We may implement Switch and Route commands here if we want to support Parking trains including backing into a siding.
	Should actually be pretty straightforward to implement but we need other Arduinos involved.
Command Type(1 char)[E | A | M | S | B | D | F | C | T | 0 | 1 ]
E = Emergency Stop All Devices(FE FF FF) (Note : all other parms in command are moot so ignore them)
A = Absolute Speed command
M = Momentum 0..7
S = Stop Immediate(this engine / train only)
B = Basic 3 - byte Legacy command(always AAAA AAA1 XXXX XXXX)
D = Railsounds Dialogue(9 - byte Legacy extended command)
F = Railsounds Effect(9 - byte Legacy extended command)
C = Effect Control(9 - byte Legacy extended command)
T = TMCC command
0 = Accessory off
1 = Accessory on

Command Parameter (1 byte) (speed, momentum, etc., or hex value from Legacy protocol tables)
*/

// Here are all the different Arduinos and their "addresses" (ID numbers) for communication.  6/25/16
const byte ARDUINO_MAS = 1;       // Master Arduino (Main controller)
const byte ARDUINO_LEG = 2;       // Output Legacy interface and accessory relay control
const byte ARDUINO_SNS = 3;       // Input reads reads status of isolated track sections on layout
const byte ARDUINO_BTN = 4;       // Input reads button presses by operator on control panel
const byte ARDUINO_SWT = 5;       // Output throws turnout solenoids (Normal/Reverse) on layout
const byte ARDUINO_LED = 6;       // Output controls the Green turnout indication LEDs on control panel
const byte ARDUINO_OCC = 7;       // Output controls the Red/Green and White occupancy LEDs on control panel

// Here are constants related to the RS485 messages
const byte RS485_MAX_LEN = 20;       // buffer length to hold the longest possible RS485 message
const byte RS485_LEN_OFFSET = 0;     // first byte of message is always total message length in bytes
const byte RS485_TO_OFFSET = 1;      // second byte of message is the ID of the Arduino the message is addressed to
const byte RS485_FROM_OFFSET = 2;    // third byte of message is the ID of the Arduino the message is coming from
									 // Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
const byte RS485_TX_CONTROL = 4;     // "transmit enable" output pin for MAX485 chip
const byte RS485_TRANSMIT = HIGH;    // HIGH = 0x1.  How to set TX_CONTROL pin when we want to transmit RS485
const byte RS485_RECEIVE = LOW;      // LOW = 0x0.  How to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

// The following lines are required by the Digole serial LCD display.
// Apparently I do *not* need this line: #include <Wire.h>
// The LCD display is connected to SERIAL #1.
#define _Digole_Serial_UART_  //To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp mydisp(&Serial1, 9600); //UART TX on arduino to RX on module

const byte SPEAKER_PIN = 2;       // Piezo buzzer pull LOW to turn on
const byte HALT_PIN = 9;          // Normally output HIGH; change to LOW to trigger A-LEG/Legacy halt (same as Emergency Stop buttons.)
const byte LED_PIN = 13;
// true = any non-zero number
// false = 0

// msgIncoming will store one incoming RS485 message.  Since we will be reading *every*
// message, most of them will probably not be for us, so we will just ignore those.
// However we need to define a byte array that is long enough to store the longest RS485
// message that any Arduino will send.  Start with a guess of 20 bytes.

byte msgIncoming[RS485_MAX_LEN];     // No need to initialize contents
byte msgOutgoing[RS485_MAX_LEN];

// *** END OF COMMON DECLARATIONS REQUIRED BY ALL ARDUINO CODE ***

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Prerequisites for Hackscribble_Ferro FRAM
//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include <Hackscribble_Ferro.h>

// 6/19/16: Modified FRAM library to change max buffer size from 64 bytes to 96 bytes
const byte FRAM1_PIN = 11;                // Digital pin 11 is CS for FRAM1, for Route Reference table used by many
const byte FRAM2_PIN = 12;                // FRAM2 used by A-LEG for Event Reference and Delayed Action tables

const byte FRAM1_ROUTE_START = 100;       // Start writing FRAM1 Route Reference table data at this address
const byte FRAM1_ROUTE_REF_LEN = 83;      // Each element of the Route Reference table is 83 bytes long
const byte FRAM1_ROUTE_RECORDS = 70;      // Number of records in Route Reference table (not including "reverse" routes not yet implemented)

const byte FRAM2_ACTION_START = 100;      // Address in FRAM2 memory to start writing our Delayed Action table
const byte FRAM2_ACTION_LEN = 12;         // Each record in the Delayed Action table is 12 bytes long
const unsigned int FRAM2_ACTION_RECORDS = 50;   // Just a guess at how many records we might need in all, probably will need 400 or so eventually

unsigned int FRAM2TopRecord = 0;          // This will keep track of the highest record number required by the Delayed Action table.

unsigned int FRAM1BufferSize;             // We will retrieve this via a function call, but it better be 96!
unsigned int FRAM2BufferSize;
unsigned long FRAM1Bottom;                // Should be 96 (address 0..95 i.e. buffer_size are reserved for any special purpose we want such as config info
unsigned long FRAM2Bottom;
unsigned long FRAM1Top;                   // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)
unsigned long FRAM2Top;

// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
// Next statement creates an instance of Ferro using our own SS pin and FRAM part number.
// You specify a FRAM part number and SS pin number like this ...
//    Hackscribble_Ferro what_we_want_to_call_it(MB85RS64V, our_CS_pin_number);
// NOTE: 6/19/16: We have changed the part number from MB85RS64 to MB85RS64V here and in the library.
// We could probably use the *standard* un-customized library and part number MB85RS64 because the
// chips are idential except for supply voltage.
// However, we have also modified the library to use a larger buffer size (default was 64 bytes) so
// since we need a modified library anyway, we might as well use the correct part number.

// Hackscribble_Ferro FRAM1(MB85RS64V, FRAM1_PIN);   // Use digital pin 11 for FRAM #1 chip select
Hackscribble_Ferro FRAM2(MB85RS64V, FRAM2_PIN);   // Use digital pin 12 for FRAM #2 chip select

// Global constants and variables used in Legacy/TMCC serial command buffer.
const byte legacyRepeats = 3;
const unsigned long legacyMinInterval = 25;   // Milliseconds between subsequent Legacy commands
const byte legacyBufElements = 100;   // Elements 0..99.  Increase if error buffer overflow.
byte legacyBuffer[legacyBufElements];  // Buffer for Legacy/TMCC commands
byte legacyBufHead = 0;   // Points to first data element of legacyBuffer[]
byte legacyBufLen = 0; // No data in buffer yet.  Max is legacyBufferSize.

// Define the structure that will hold individual Delayed Action table records
// IMPORTANT REGARDING parm2.  I can't think of when we would use parm2, unless we need it as a flag or something for the
// special case when we trip a destination entry sensor and must convert a Sensor record to a (delayed) Timer record???

struct delayedAction {
	char status;                    // Sensor-type, Time-type, or Expired
	byte sensorNo;                  // 0 if n/a (Time-type), or 1..52
	char sensorType;                // Trip or Clear
	unsigned long timeRipe;         // 0 if n/a (Sensor-type) or time in millis() to execute this record
	char deviceType;                // Engine, Train, Accessory, Switch, or Route (usually E or T)
	byte deviceNumber;              // Engine or train number, or accessory number, etc.
	char commandType;               // E, A, M, S, B, D, F, C, T,  or (Y or 0/1 or accessorY vs Acc'y On/Off.)
	byte parm1;
	byte parm2;						// Possibly never used
};

// Create an array of Delayed Action records
// Note that we will NOT have a similar array in the final code; we are creating this as a means of populating the table into FRAM2

delayedAction actionElement;  // Use this to hold individual elements when retrieved
delayedAction actionArray[FRAM2_ACTION_RECORDS] = {   // There are currently 50 records in the Delayed Action table
	{ 'T',0,' ',5000,'E',80,'B',251,0 },   // Long start-up sequence
	{ 'T',0,' ',20000,'E',80,'D',8,0 },    // Engineer request permission to depart
	{ 'T',0,' ',33000,'E',80,'B',245,0 },  // Turn on bell
	{ 'T',0,' ',36000,'E',80,'B',165,0 },  // RPMs 5
	{ 'T',0,' ',37000,'E',80,'M',4,0 },    // Momentum 4
	{ 'T',0,' ',37000,'E',80,'A',120,0 },  // Abs speed 120
	{ 'T',0,' ',39000,'E',80,'B',244,0 },  // Bell off
	{ 'T',0,' ',41000,'E',80,'B',28,0 },   // Blow horn
	{ 'T',0,' ',41200,'E',80,'B',28,0 },
	{ 'T',0,' ',41400,'E',80,'B',28,0 },
	{ 'T',0,' ',41600,'E',80,'B',28,0 },
	{ 'T',0,' ',41800,'E',80,'B',28,0 },
	{ 'T',0,' ',43000,'E',80,'B',160,0 },  // RPMs all the way down
	{ 'T',0,' ',43500,'E',80,'A',0,0 },    // Abs speed zero
	{ 'T',0,' ',51000,'E',80,'B',253,0 },
	{ 'T',0,' ',55000,'E',80,'B',254,0 },
	{ 'T',0,' ',5100,'E',1,'T',23,0 },     // 23 = numberic 7 = departure announcement (w/out Aux 1)
	{ 'T',0,' ',10000,'E',1,'T',9,0 },     // 9 = Aux 1
	{ 'T',0,' ',10050,'E',1,'T',23,0 },    // 23 = numeric 7 = arrival announcement (w/ Aux 1)
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 },
	{ 'E',0,' ',0,'E',0,' ',0,0 }
};

// Define global bytes for each of the 9 possible used in Legacy commands.
// Most commands only use simple 3-byte Legacy commands.
// Nine-byte commands are for Dialogue, Fx, and Control "extended" commands.

byte legacy11 = 0x00;   // F8 for Engine, or F9 for Train (or FE for emergency stop all or TMCC)
byte legacy12 = 0x00;
byte legacy13 = 0x00;
byte legacy21 = 0xFB;   // Always FB for 2nd and 3rd "words", when used
byte legacy22 = 0x00;
byte legacy23 = 0x00;
byte legacy31 = 0xFB;  // Always FB for 2nd and 3rd "words", when used
byte legacy32 = 0x00;
byte legacy33 = 0x00;   // This will hold checksum for "multi-word" commands

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

	Serial.begin(115200);              // PC serial monitor window
	// Serial1 is for the Digole 20x4 LCD debug display, already set up
	Serial2.begin(115200);             // RS485 can run at 115200 baud
	Serial3.begin(9600);             // Legacy serial interface

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

	pinMode(RS485_TX_CONTROL, OUTPUT);    // Will get set LOW or HIGH depending on RS-485 read or write
	digitalWrite(RS485_TX_CONTROL, RS485_RECEIVE);  // Receive mode - any time we aren't transmitting
	pinMode(LED_PIN, OUTPUT);             // built-in LED
	digitalWrite(RS485_TX_CONTROL, LOW);  // make sure LED is turned off
	pinMode(SPEAKER_PIN, OUTPUT);         // Piezo buzzer pin
	digitalWrite(SPEAKER_PIN, HIGH);      // make sure piezo speaker is off

    // Now create our FRAM objects and make sure they are working properly
	// Repeat these tests for FRAM1 when we start using it

	if (FRAM2.begin()) {       // Any non-zero result is an error
		Serial.println("FRAM2 response not OK");
		sendToLCD("FRAM2 Bad.");
		endWithFlashingLED(7);         // 7 flashes = FRAM problem
	}
	Serial.println("FRAM2 response OK.");
	sendToLCD("FRAM2 Good.");

	FRAM2BufferSize = FRAM2.getMaxBufferSize();  // Should return 96 w/ modified library value
	FRAM2Bottom = FRAM2.getBottomAddress();      // Should return 96 also
	FRAM2Top = FRAM2.getTopAddress();            // Should return 8191

	Serial.print("FRAM2 Buffer size: ");
	Serial.println(FRAM2BufferSize);    // result = 96 (with modified library)  Old library would be 64
	Serial.print("FRAM2 Bottom: ");
	Serial.println(FRAM2Bottom);        // result = 96
	Serial.print("FRAM2 Top: ");
	Serial.println(FRAM2Top);           // result = 8191

	if ((FRAM2BufferSize != 96) || (FRAM2Bottom != 96) || (FRAM2Top != 8191)) {        // If buffer size is not 96, then we are using an un-modified version of the library!
		Serial.println("FRAM2 buffer size, bottom, or top error!  Likely library is not the modified version.");
		sendToLCD("FRAM2 bad size.");
		endWithFlashingLED(7);          // 7 flashes = FRAM problem
	}

	// NOW WE HAVE SPECIAL CODE TO POPULATE FRAM2 WITH DELAYED ACTION RECORDS CREATED ABOVE

	Serial.println("Now write the entire Delayed Action table to FRAM2.");
	Serial.print("Start milliseconds ...");
	Serial.println(millis());

	for (unsigned int delayedActionRecord = 0; delayedActionRecord < FRAM2_ACTION_RECORDS; delayedActionRecord++) {  // Using offset 0 for ease of calcs

		unsigned long FRAM2Address = FRAM2_ACTION_START + (delayedActionRecord * FRAM2_ACTION_LEN);

		// FRAM2.write requires a  buffer of type BYTE, not char or structure, so convert
		// Addresses must be specified in UNSIGNED LONG (not UNSIGNED INT)
		byte b[FRAM2_ACTION_LEN];  // create a byte array to hold one Route Reference record
		memcpy(b, &actionArray[delayedActionRecord], FRAM2_ACTION_LEN);
		FRAM2.write(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_write, data
	}

	Serial.println(millis());
	Serial.println("Done with writing Delayed Actions to FRAM2.");
}

// ***************************************************************************************
// **************************************  L O O P  **************************************
// ***************************************************************************************

void loop() {

	// Scan the Delayed Action table starting at the beginning for records that qualify for action (currently, just based on time)
	// As soon as we find any qualifying record, we'll flag it as Expired, exit the search loop, and proceed to the "execute a command" code.
	// If this is an EXPIRED record, then just move on to the next record.
	// If this is an ACTIVE, TIMED record and our time has not yet come up, then skip the record and move on
	// If this is an ACTIVE, TIMED record that is RIPE, the UPDATE the record as Expired, and EXECUTE the command (train speed, etc.)
	// Note that we exit the loop as soon as we find a ripe record, always starting at the beginning.

	static unsigned int highestValidDelayedActionRecord = 0;  // Will keep track of max table size needed, only initialize once (STATIC)

	byte actionRecordFound = false;     // Set this to true if we find a record in the following table scan

	// IMPORANT: We eventually need to change the following for.. loop to not scan the entire table, but only the highest active record.
	// We defined a variable for this above, highestValidDelayedActionRecord = 0, be we need to establish a value as we populate the table
	// and as we add records (if they must be added to the top of the table rather than an expired slot.)

	for (unsigned int delayedActionRecord = 0; delayedActionRecord < FRAM2_ACTION_RECORDS; delayedActionRecord++) {  // Using offset 0 for ease of calcs

		// FRAM addresses must be UNSIGNED LONG
		unsigned long FRAM2Address = FRAM2_ACTION_START + (delayedActionRecord * FRAM2_ACTION_LEN);
		byte b[FRAM2_ACTION_LEN];  // create a byte array to hold one Delayed Action record
		FRAM2.read(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_read, data
		memcpy(&actionElement, b, FRAM2_ACTION_LEN);

		// Now actionElement has been populated with the record just read.

		if (actionElement.status == 'E') {            // Expired record.  Skip and move on.
		}

		else if (actionElement.status == 'S') {       // Sensor-type record, we don't have logic for that yet.
			// Sensor logic goes here.
			// NOTE: We have to deal with the case where a train hits a destination entry sensor, and must slow down AFTER a calculated
			// delay (to let the train progress into a long siding far enough, so that when it slows down it will hit the target exit-sensor
			// trip speed of 20 or whatever.)  We can calculate the delay in advance, but not the time to start the delay because that begins
			// when the train hits the destination entry sensor and we can't know exactly what time that will be.  So we will have a special
			// kind of record that is a Sensor Trip with pre-calculated delay before slowing down.  We will simply update the record from a
			// Sensor type record to a Timer record, with the time set as current time plus delay that was specified as a parameter in the
			// original record.  So it only amounts to updating the record.  We leave actionRecordFound false and just move on, and will
			// eventually encounter this record again and process it when the specified millis() time has expired.

		}

		else if ((actionElement.status == 'T') && (actionElement.timeRipe > millis())) {   // Time record, but not yet ripe so move on.
		}

		else if ((actionElement.status == 'T') && (actionElement.timeRipe <= millis())) {  // Time record and it's ripe!
			// Write back the record as "Expired" since we're going to process it now.
			actionElement.status = 'E';    // Set new status of this record to Expired and update the record in FRAM2
			memcpy(b, &actionElement, FRAM2_ACTION_LEN);
			FRAM2.write(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_write, data

			actionRecordFound = true;   // Tells the code following this "for" loop to process actionElement.
			break;                      // Bail out of this "for" loop and process the valid element
		}
	}   // End of for...try to find the first applicable record in the Delayed Action table loop

	// If we have a valid record to process in actionElement, then actionRecordFound will be "true"

	if (actionRecordFound == true) {        // We have a valid record to process!

		// IMPORTANT: We need to add code here to populate a buffer that's big enough to hold a bunch of commands,
		// and then call a function to not send them with less than 30ms between.
		// I guess the way to do that is every time we go through the loop, we call the function and if the buffer
		// is not empty, and if sufficient time has passed, then send the next command in the buffer (and move the
		// head/tail pointers as appropriate.)
		// Separate functions to add to the buffer, versus calling the buffer.
		// Even though we only process one delayedAction record for each iteration through the loop, it's entirely
		// possible that more than one record will have the same timestamp to execute, and since our loop is so fast
		// that would put us in the position of potentially sending Legacy commands less than 30ms apart.

		Serial.print("We have a valid record to process!  Current time: ");
		Serial.println(millis());
		Serial.print(actionElement.status);
		Serial.print(", ");
		Serial.print(actionElement.sensorNo);
		Serial.print(", ");
		Serial.print(actionElement.sensorType);
		Serial.print(", ");
		Serial.print(actionElement.timeRipe);
		Serial.print(", ");
		Serial.print(actionElement.commandType);
		Serial.print(", ");
		Serial.print(actionElement.deviceType);
		Serial.print(", ");
		Serial.print(actionElement.deviceNumber);
		Serial.print(", ");
		Serial.print(actionElement.parm1);
		Serial.print(", ");
		Serial.println(actionElement.parm2);

		// actionElement.deviceType (1 char) Engine or Train or blank if  n/a
		// actionElement.deviceNumber (1 byte) Engine, Train, Acc'y, Switch, or Route number
		// actionElement.commandType (1 char) [E|A|M|T|S|B|D|F|C|0|1]
		//	 E = Emergency Stop All Devices(FE FF FF) (Note : all other parms in command are moot so ignore them)
		//	 A = Absolute Speed command
		//	 M = Momentum 0..7
		//	 S = Stop Immediate(this engine / train only)
		//   B = Basic 3 - byte Legacy command(always AAAA AAA1 XXXX XXXX)
		//   D = Railsounds Dialogue(9 - byte Legacy extended command)
		//   F = Railsounds Effect(9 - byte Legacy extended command)
		//   C = Effect Control(9 - byte Legacy extended command)
		//   T = TMCC command
		//   0 = Accessory off
		//   1 = Accessory on


		// actionElement.parm1 (1 byte) Hex value from Legacy protocol table, if applicable.
		//   N/a for CmdTypes E and S.

		switch (actionElement.commandType) {
			case 'E':  // Emergency stop all devices
				Serial.println("Emergency Stop.");
				sendToLCD("Emergency Stop All.");
				legacy11 = 0xFE;
				legacy12 = 0xFF;
				legacy13 = 0xFF;
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
			case 'A':   // Set absolute speed
				Serial.println("Absolute speed command.");
				if (actionElement.deviceType == 'E') {
					legacy11 = 0xF8;
				}
				else {
					legacy11 = 0xF9;
				}
				legacy12 = actionElement.deviceNumber * 2;  // Shift left one bit, fill with zero
				legacy13 = actionElement.parm1;
				sendToLCD(actionElement.deviceType + String(actionElement.deviceNumber) + " A.S. " + String(actionElement.parm1));
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
			case 'M':   // Set momentum
				Serial.println("Momentum.");
				if (actionElement.deviceType == 'E') {
					legacy11 = 0xF8;
				}
				else {
					legacy11 = 0xF9;
				}
				legacy12 = actionElement.deviceNumber * 2;  // Shift left one bit, fill with zero
				legacy13 = 0xC8 + actionElement.parm1;
				sendToLCD(actionElement.deviceType + String(actionElement.deviceNumber) + " Mom " + String(actionElement.parm1));
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
			case 'S':    // Stop immediate (this engine or train)
				Serial.println("Stop immediate.");
				if (actionElement.deviceType == 'E') {
					legacy11 = 0xF8;
				}
				else {
					legacy11 = 0xF9;
				}
				legacy12 = actionElement.deviceNumber * 2;  // Shift left one bit, fill with zero
				legacy13 = 0xFB;
				sendToLCD(actionElement.deviceType + String(actionElement.deviceNumber) + " Stop immed.");
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
			case 'B':
				Serial.println("Basic Legacy 3-byte command.");
				if (actionElement.deviceType == 'E') {
					legacy11 = 0xF8;
				}
				else {
					legacy11 = 0xF9;
				}
				legacy12 = (actionElement.deviceNumber * 2) + 1;  // Shift left one bit, fill with '1'
				legacy13 = actionElement.parm1;
				sendToLCD(actionElement.deviceType + String(actionElement.deviceNumber) + " Basic " + String(actionElement.parm1));
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
			case 'D':
			case 'F':
			case 'C':
				Serial.println("Railsounds D/F/C 9-byte command.");
				if (actionElement.deviceType == 'E') {
					legacy11 = 0xF8;
				}
				else {
					legacy11 = 0xF9;
				}
				legacy12 = (actionElement.deviceNumber * 2) + 1;  // Shift left one bit, fill with '1'
				if (actionElement.commandType == 'D') {
					legacy13 = 0x72;   // Railsounds Dialog Trigger
				}
				else if (actionElement.commandType == 'F') {
					legacy13 = 0x74;   // Railsounds Effecs Triggers
				}
				else if (actionElement.commandType == 'C') {
					legacy13 = 0x7C;   // Effects Controls
				}
				// Note: legacy21 and legacy31 are pre-defined; always "FB"
				legacy22 = (actionElement.deviceNumber * 2);   // If it's an Engine, okay as-is
				if (actionElement.deviceType == 'T') {     // If it's a Train, set bit to "1"
					legacy22 = legacy22 + 1;
				}
				legacy23 = actionElement.parm1;
				legacy32 = legacy22;
				// Lionel Legacy command checksum.
				legacy33 = legacyChecksum(legacy12, legacy13, legacy22, legacy23, legacy32);
				sendToLCD(actionElement.deviceType + String(actionElement.deviceNumber) + " Ext Cmd " + String(actionElement.parm1));
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
					legacyBufPut(legacy21);
					legacyBufPut(legacy22);
					legacyBufPut(legacy23);
					legacyBufPut(legacy31);
					legacyBufPut(legacy32);
					legacyBufPut(legacy33);
				}
				// Don't bother with legacyBufProcess() / immediate execution since we have 3 sets and not urgent anyway
				break;
			case 'T':  // TMCC command (only Action types supported here, and only Engines not Trains)
				Serial.println("TMCC Action.");
				sendToLCD("TMCC Action.");
				legacy11 = 0xFE;
				legacy12 = (actionElement.deviceNumber >> 1);  // Could just do logical && with 11001000 to make it Train not Engine
				legacy13 = (actionElement.deviceNumber << 7) + actionElement.parm1;
				for (byte i = 1; i <= legacyRepeats; i++) {
					legacyBufPut(legacy11);
					legacyBufPut(legacy12);
					legacyBufPut(legacy13);
				}
				legacyBufProcess();  // attempt immediate execution
				break;
		}    // end of switch block
				sendToLCD(String(legacy11, HEX) + " " + String(legacy12, HEX) + " " + String(legacy13, HEX));
	}   // end of "if we have a Delayed Action table record to process block

	// VERY IMPORTANT to put a call to legacyBufProcess() right after this loop, and repeat every time following above
	// code to populate legacy buffer, because some commands might populate the buffer at almost the same time, and not
	// be able to be executed until legacyMinInterval has passed.  Also the 3-byte extended commands require basically
	// three times through this loop() in order for all three sets of 3 bytes to be sent to Legacy.
	legacyBufProcess();  // Send 3 bytes to Legacy, if available in buffer.

}   // end of loop()

// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

byte legacyChecksum(byte leg12, byte leg13, byte leg22, byte leg23, byte leg32) {
	// Rev 6/26/16
	// Calculate the checksum required when sending "multi-word" commands to Legacy.
	// This function receives the five bytes from a 3-word Legacy command that are
	// used in calculation of the checksum, and returns the checksum.
	// Add up the five byte values, take the MOD256 remainder (which is automatically
	// done by virtue of storing the result of the addition in a byte-size variable,)
	// and then take the One's Complement which is simply inverting all of the bits.
	// We use the "~" operator to do a bitwise invert.

	return ~(leg12 + leg13 + leg22 + leg23 + leg32);
}

// Legacy command buffer functions: Put, Get, and Process.
// The buffer is legacyBufSize elements (bytes) long (i.e. 8 bytes means there are elements 0..7)
// The active data begins at element legacyBufHead (element 0..7 in this example)
// The number of active elements is legacyBufLen.  Cannot exceed legacyBufSize i.e. 8 bytes in this example
// legacyBufPut adds a byte to the buffer, and terminates if the buffer overflows (buffer size exceeded).
// legacyBufGet removes/returns a byte from the buffer, increments the head, and decrements the length.
// legacyBufProcess sends a 3-byte command (TMCC or Legacy) out the serial port to Legacy.

void legacyBufPut(byte d) {
	// Insert a byte at the end of the Legacy command serial buffer, for later transmission to Legacy
	// Uses global variables: legacyBuffer[], legacyBufLen, legacyBufSize, legacyBufHead
	if (legacyBufLen < legacyBufElements) {
		legacyBuffer[(legacyBufHead + legacyBufLen) % legacyBufElements] = d;
		legacyBufLen++;
		Serial.print("Putting byte in buffer: "); Serial.print(d, HEX); Serial.print(" at location: ");
		Serial.print((legacyBufHead + legacyBufLen) % legacyBufElements); Serial.print(", new length: ");
		Serial.println(legacyBufLen);
	} else {       // buffer overflow
		sendToLCD("Legacy buf overflow.");
		endWithFlashingLED(6);
	}
}

byte legacyBufGet() {
	// Retrieve a byte from the head of the Legacy command serial buffer, to be transmitted to Legacy
	// Uses global variables: legacyBuffer[], legacyBufLen, legacyBufSize, legacyBufHead
	byte b = 0;
	if (legacyBufLen > 0) {
		b = legacyBuffer[legacyBufHead];
		// Increase head and decrease Len
		legacyBufHead = (legacyBufHead + 1) % legacyBufElements;
		legacyBufLen--;
//		Serial.print("Getting from buffer: "); Serial.print(b, HEX); Serial.print(", new head: "); Serial.print(legacyBufHead);
//		Serial.print(", new length: "); Serial.println(legacyBufLen);
	} else {
		endWithFlashingLED(7);  // Should never hit this!
	}
	return b;
}

void legacyBufProcess() {
	// If minimum interval has elapsed, transmit one 2- or 3-byte TMCC or Legacy command.
	// TMCC commands are two bytes long and Legacy commands are three bytes long, so we have to check before transmitting.
	static unsigned long legacyLastTransmit = millis();
	byte b;
	if (legacyBufLen == 0) return;
	if ((millis() - legacyLastTransmit) > legacyMinInterval) {
		Serial.print("Writing to Legacy: ");
		b = legacyBufGet();
		if (b==0xF8 || b==0xF9 || b==0xFB) {    // It's a Legacy command
			Serial.print("It's a Legacy command: ");
		} else if (b == 0xFE) {    // It's a TMCC command
			Serial.print("It's a TMCC command: ");
		} else {
			Serial.print("FATAL ERROR.  Unexpected data in Legacy buffer: ");
			Serial.println(b, HEX);
			while (true);
		}
		Serial.print(b, HEX); Serial.print(" ");
		Serial3.write(b);      // Write 1st of 3 bytes
		b = legacyBufGet();    // Get the 2nd byte of 3
		Serial.print(b, HEX); Serial.print(" ");
		Serial3.write(b);      // Write 2nd of 3 bytes
		b = legacyBufGet();    // Get the 3rd byte of 3
		Serial.print(b, HEX); Serial.print(" Buf len is now: "); Serial.println(legacyBufLen);
		Serial3.write(b);      // Write 3rd of 3 bytes
		legacyLastTransmit = millis();
	}
}






// ***************************************************************************
// *** HERE ARE FUNCTIONS USED BY VIRTUALLY ALL ARDUINOS *** REV: 07-07-16 ***
// ***************************************************************************

void getRS485Message(byte tMsg[]) {
	// NEEDS TO BE UPDATED TO TURN ON RECEIVE LED
  // Rev 6/26/16
	// This function requires that there is at least one byte in the incoming serial buffer,
	// and it will wait until an entire incoming message is received, unless it times out.
	// This only reads and returns one message at a time, regardless of how much more data
	// may be waiting in the serial buffer.
	// Input byte tmsg[] is the initialized incoming byte array whose contents will be filled.
	// tmsg[] is also "returned" by the function since arrays are passed by reference.
	// If this function returns, then we are guaranteed to have a real/accurate message in the buffer.
	// However, the function does not check if it is to "us" (this Arduino) or not.

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

void sendToLCD(String s) {
	// Rev 6/26/16
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

void endWithFlashingLED(int numFlashes) {
	// Rev 7/7/16
	// Flash the on-board LED a given number of times, forever.
	// Use this when program must terminate due to critical error.
	while (true) {
		for (int i = 1; i <= numFlashes; i++) {
			digitalWrite(LED_PIN, HIGH);
			digitalWrite(SPEAKER_PIN, LOW);  // turn on piezo
			delay(100);
			digitalWrite(LED_PIN, LOW);
			digitalWrite(SPEAKER_PIN, HIGH);
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
