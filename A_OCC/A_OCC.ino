// A_OCC Rev: 10/01/17.
char APPVERSION[21] = "A-OCC Rev. 10/01/17";

// Include the following #define if we want to run the system with just the lower-level track.
#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

// A-OCC controls the white occupancy LEDs and RGB block LEDs on the control panel via Centipede shift registers.
// A-OCC also prompts operator for train info during registration.
// A-OCC also plays WAV files to P.A. system as trains depart and arrive at stations (autonomously.)
// A-OCC is primarily an "INPUT-ONLY" module that does not provide data to any other Arduino, except during registration.
// In Auto and Park modes, the operator can turn the rotary encoder and that will trigger A-OCC to "re-paint" the control
// panel LEDs, so that the selected train (if any, static = none) will be lit in red instead of blue.
// Control red/blue control panel block reservation and occupancy LEDs via Centipede 1 (address 0.)
// Control white control panel occupancy-indication LEDs via Centipede 2 (address 1.)
// Listens for RS485 commands from A-SNS to A-MAS to track occupancy sensor status (just like A-MAS does.)
// Listens for RS485 commands from A-MAS to A-LEG to track route reservations (and release based on A-SNS messages.)
// Communicates bidirectionally via RS485 with A-MAS during Registration, to prompt operator for starting train locations.
// BLUE AND RED BLOCK OCCUPANCY/RESERVATION LEDS:
// Since blue blends into the background, and red stands out, we'll want to use red to emphasize something.
// In MANUAL and POV mode, *all* occupied blocks will display a SOLID BLUE LED.
// In REGISTER mode, all occupied blocks except the one being identified (by train name/number) will be SOLID BLUE,
//   and the train at the block being identified (prompted) will be BLINKING RED.  No need for the operator to know/see
//   which occupied blocks have/haven't been ID'd yet, because we won't stop prompting until all have been ID'd.
// In AUTO and PARK mode, all non-selected (by rotary encoder to ID train name/number) blocks will be BLUE.
//   Non-selected reserved routes will be SOLID BLUE, and the block(s) with the train in it (the tail) will be BLINKING BLUE.
//   The selected (by rotary encoder to ID the train name/number) train's reserved route will be SOLID RED, and the block(s)
//   with the selected train in it (the tail) will be BLINKING RED.

// IMPORTANT: A-MAS really needs to send PA commands to A-OCC, rather than letting A-OCC decide, because A-MAS should tell A-OCC to make
// an announcement i.e. departing well BEFORE the train starts moving out.  How would A-OCC know when this would be?  Unless we want A-MAS
// to send DELAYED DEPARTURE commands to A-LEG that A-OCC would see...  Thus A-OCC could make the announcement as soon as it sees that
// A-MAS has told A-LEG to depart...  But that would result in possibly unnecessary delays that A-MAS would need to impose, even when
// A-OCC might not make an announcement for that train, for whatever reason...

// 09/16/17: Changed variable suffixes from Length to Len, No/Number to Num, Record to Rec, etc.
// 02/26/17: Added mode and state changed = false at top of get RS485 message main loop
// STILL TO BE DONE:
//   Add code for AUTO and PARK mode that illuminates control panel occupancy and block LEDs as appropriate.  This will
//     require us to maintain a Train Progress table and monitor train route commands from A-MAS to A-LEG, including
//     block reservations per the Route Reservation (and Park 1, Park 2) table.
//   Add code to manage the P.A. Announcement system, when running in Auto mode (and optionally Park mode.)
// 02/25/17: Adding Rotary Encoder logic to prompt operator, with 8-char A/N display.
// 01/21/17: Adding registration code.
// 01/13/17: Added a few pin numbers i.e. rotary encoder and other misc as we work towards completing this code.
// 11/11/16: Update code to reflect that in MANUAL, POV, and REGISTER modes, there is no STOPPING state.
// Go directly from RUNNING to STOPPED.  No point in STOPPING.
// 11/10/16. Initial version.

// CENTIPEDE #1, Address 0: RED/BLUE BLOCK INDICATOR LEDS
// Centipede pin 0 corresponds with block 1 blue
// Centipede pin 15 corresponds with block 16 blue
// Centipede pin 16 corresponds with block 1 red
// Centipede pin 31 corresponds with block 16 red
// Centipede pin 32 corresponds with block 17 blue
// Centipede pin 47 corresponds with block 32 blue   // though there are only 26
// Centipede pin 48 corresponds with block 17 red
// Centipede pin 63 corresponds with block 32 red  // though there are only 26

// CENTIPEDE #2, Address 1: WHITE OCCUPANCY SENSOR LEDS
// Centipede pin 64 corresponds with white LED occupancy sensor 1
// Centipede pin 79 corresponds with white LED occupancy sensor 16
// Centipede pin 80 corresponds with white LED occupancy sensor 17
// Centipede pin 95 corresponds with white LED occupancy sensor 32
// Centipede pin 96 corresponds with white LED occupancy sensor 33
// Centipede pin 111 corresponds with white LED occupancy sensor 48
// Centipede pin 112 corresponds with white LED occupancy sensor 49
// Centipede pin 127 corresponds with white LED occupancy sensor 64   // though there are only 52

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-OCC.  Byte numbers represent offsets, so they start at zero. ***
// Because there are so many message types, we will document the protocols here and just use integers for offsets in the code.

// A-MAS BROADCAST: Mode change
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//   	0	  Length	  Byte	7
//   	1	  To	      Byte	99 (ALL)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'M' means this is a Mode/State update command
//   	4	  Mode	    Byte  1..5 [Manual | Register | Auto | Park | POV]
//   	5	  State	    Byte  1..3 [Running | Stopping | Stopped]
//   	6	  Cksum	    Byte  0..255

// A-SNS to A-MAS:  Sensor status update for a single sensor change
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 7
//    1	   To	       Byte	 1 (A_MAS)
//    2	   From	     Byte	 3 (A_SNS)
//    3    Command   Char  'S' Sensor status update
//    4    Sensor #  Byte  1..53 (Note that as of Sept 2017, we have disconnected sensor 53 from the layout)
//    5    Trip/Clr  Byte  [0|1] 0-Cleared, 1=Tripped
//    6	   Checksum	 Byte  0..255

// A-MAS to A-OCC: Query for operator ANSWER QUESTION via alphanumeric display.  Registration mode only.
// Rev: 09/20/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  14
//   	1	  To	      Byte	7 (A-OCC)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'Q' means it's a question requires operator to select one displayed choice
//    4   Text      Char  8 character prompt i.e. "SMOKE Y"/"SMOKE N", "FAST ON"/"SLOW ON", "AUDIO Y"/"AUDIO N"
//  thru  ...       ...   ...
//   11
//   12   Last?     Char  'N' means there will be another question coming; 'Y' means all done with questions from A-MAS
//   13	  Cksum	    Byte  0..255

// A-OCC to A-MAS: Reply from operator with ANSWERED QUESTION via alphanumeric display.  Registration mode only.
// Rev: 09/20/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  6
//   	1	  To	      Byte	1 (A-MAS)
//   	2	  From	    Byte	7 (A_OCC)
//    3   Msg Type  Char  'Q' means this is a reply to the Question that A-MAS "asked" via A-OCC's 8-char A/N display and rotary encoder.
//    4   Reply #   Byte  0..n, where 0 was the first prompt sent by A-MAS, and so on.  I.e. zero offset, not starting with 1.
//    5   Cksum     Byte  0..255

// A-MAS to A-OCC: Query for operator to REGISTER TRAIN locations.  Registration mode only.
// Rev: 09/20/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  16
//   	1	  To	      Byte	7 (A-OCC)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'R' means Registration!  Ask operator to ID trains in each occupied block.
//    4   Train No. Byte  [1..MAX_TRAINS]  (Yes, this is actual train number i.e. #1; there is no "real" train 0.)
//    5   Train Dsc Char  8 character train "name" (from Train Reference table) prompt i.e. "WP 2345"
//  thru  ...       ...   ...
//   12
//   13   Last Blk  Byte  0..TOTAL_BLOCKS.  Last-known block that this train occupied, or 0 if unknown.  Used as default.
//   14   Last?     Char  'N' means there will be another message coming; 'Y' means all done sending list of all train prompts.
//   15	  Cksum	    Byte  0..255

// A-OCC to A-MAS: Reply REGISTERED TRAINS from operator via alphanumeric display.  Registration mode only.
// Rev: 09/27/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  9
//   	1	  To	      Byte	1 (A-MAS)
//   	2	  From	    Byte	7 (A_OCC)
//    3   Msg Type  Char  1 'R' means train Registration reply. This will always be a "real" train, never zero, never TRAIN_STATIC or TRAIN_DONE.
//    4   Train No. Byte  [1..MAX_TRAINS]  Can only be zero if this is the last record, which contains no train data.
//    5   Block No. Byte  [1..TOTAL_BLOCKS]  Can only be zero if this is the last record, which contains no train data.
//    6   Block Dir Char  [E|W] Indicates which direction the train is facing withing that block.  Known based on which block sensor is tripped.
//    7   Last?     Char  'N' means there will be another identified train message coming; 'Y' means we are done and no train data in this record.
//    8	  Cksum	    Byte  0..255

// A-MAS to A-OCC: Command to play a WAV file on the PA system, such as "Train 3945 arriving on track 5!"
// Rev: 09/20/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  13
//   	1	  To	      Byte	7 (A-OCC)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'P' means it's a PA announcement command
//    4   Phrase 1  Byte  Phrase number stored in WAV Trigger micro SD card.  i.e. 1 = "Wester Pacific Scenic Limited"
//    5   Phrase 2  Byte  I.e. 13 = "Now departing Science City"
//    6   Phrase 3  Byte  I.e. 52 = "Track number two"
//    7   Phrase 4  Byte  I.e. 109 = "Aaaaaaall abooooooard!"
//    8   Phrase 5  Byte  0 = Unused.  Cound also have special commands for pause.
//    9   Phrase 6  Byte  0 = Unused.  Maybe "Please watch your step!"
//   10   Phrase 7  Byte  0 = Unused.
//   11   Phrase 8  Byte  0 = Unused.
//   12   Checksum  Byte  0..255

// **************************************************************************************************************************

// *** ARDUINO DEVICE CONSTANTS: Here are all the different Arduinos and their "addresses" (ID numbers) for communication.
const byte ARDUINO_NUL =  0;  // Use this to initialize etc.
const byte ARDUINO_MAS =  1;  // Master Arduino (Main controller)
const byte ARDUINO_LEG =  2;  // Output Legacy interface and accessory relay control
const byte ARDUINO_SNS =  3;  // Input reads reads status of isolated track sections on layout
const byte ARDUINO_BTN =  4;  // Input reads button presses by operator on control panel
const byte ARDUINO_SWT =  5;  // Output throws turnout solenoids (Normal/Reverse) on layout
const byte ARDUINO_LED =  6;  // Output controls the Green turnout indication LEDs on control panel
const byte ARDUINO_OCC =  7;  // Output controls the Red/Green and White occupancy LEDs on control panel
const byte ARDUINO_ALL = 99;  // Master broadcasting to all i.e. mode change

// *** ARDUINO PIN NUMBERS: Define Arduino pin numbers used...specific to A-OCC
const byte PIN_ROTARY_1        =  2;  // Input: Rotary Encoder pin 1 of 2 (plus Select)
const byte PIN_ROTARY_2        =  3;  // Input: Rotary Encoder pin 2 of 2 (plus Select)
const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
const byte PIN_RS485_TX_LED    =  5;  // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
const byte PIN_RS485_RX_LED    =  6;  // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
const byte PIN_SPEAKER         =  7;  // Output: Piezo buzzer connects positive here
const byte PIN_HALT            =  9;  // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF
const byte PIN_WAV_TRIGGER     = 10;  // Input: LOW when a track is playing, else HIGH.
const byte PIN_FRAM1           = 11;  // Digital pin 11 is CS for FRAM1, for Route Reference table used by many, and last-known-state of all trains
const byte PIN_FRAM2           = 12;  // FRAM2 used by A-LEG for Event Reference and Delayed Action tables
const byte PIN_LED             = 13;  // Built-in LED always pin 13
const byte PIN_ROTARY_PUSH     = 19;  // Input: Rotary Encoder "Select" (pushbutton) pin

// *** MODE AND STATE DEFINITIONS: We'll need to keep track of what mode we are in.
const byte MODE_UNDEFINED  = 0;
const byte MODE_MANUAL     = 1;
const byte MODE_REGISTER   = 2;
const byte MODE_AUTO       = 3;
const byte MODE_PARK       = 4;
const byte MODE_POV        = 5;
const byte STATE_UNDEFINED = 0;
const byte STATE_RUNNING   = 1;
const byte STATE_STOPPING  = 2;
const byte STATE_STOPPED   = 3;
// We will start in MODE_UNDEFINED, STATE_STOPPED.  We must receive a message from A-MAS to tell us otherwise.
byte modeCurrent = MODE_UNDEFINED;
byte modeOld = modeCurrent;
bool modeChanged = false;  // false = 0; true = any non-zero number
byte stateCurrent = STATE_STOPPED;
byte stateOld = stateCurrent;
bool stateChanged = false;

// *** SERIAL LCD DISPLAY: The following lines are required by the Digole serial LCD display, connected to serial port 1.
const byte LCD_WIDTH = 20;        // Number of chars wide on the 20x04 LCD displays on the control panel
#define _Digole_Serial_UART_      // To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp LCDDisplay(&Serial1, 115200); //UART TX on arduino to RX on module
char lcdString[LCD_WIDTH + 1];    // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** RS485 MESSAGES: Here are constants and arrays related to the RS485 messages
// Note that the serial input buffer is only 64 bytes, which means that we need to keep emptying it since there
// will be many commands between Arduinos, even though most may not be for THIS Arduino.  If the buffer overflows,
// then we will be totally screwed up (but it will be apparent in the checksum.)
const byte RS485_MAX_LEN     = 20;    // buffer length to hold the longest possible RS485 message.  Just a guess.
      byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
      byte RS485MsgOutgoing[RS485_MAX_LEN];
const byte RS485_LEN_OFFSET  =  0;    // first byte of message is always total message length in bytes
const byte RS485_TO_OFFSET   =  1;    // second byte of message is the ID of the Arduino the message is addressed to
const byte RS485_FROM_OFFSET =  2;    // third byte of message is the ID of the Arduino the message is coming from
// Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
const byte RS485_TRANSMIT    = HIGH;  // HIGH = 0x1.  How to set TX_CONTROL pin when we want to transmit RS485
const byte RS485_RECEIVE     = LOW;   // LOW = 0x0.  How to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

// *** SHIFT REGISTER: The following lines are required by the Centipede input/output shift registers.
#include <Wire.h>                 // Needed for Centipede shift register
#include <Centipede.h>
Centipede shiftRegister;          // create Centipede shift register object

// *** FRAM MEMORY MODULE:  Constants and variables needed for use with Hackscribble Ferro RAM.
// FRAM is used to store the Route Reference Table, the Delayed Action Table, and other data.
// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
// IMPORTANT: 7/12/16: Modified FRAM library to change max buffer size from 64 bytes to 128 bytes.
// As of 10/18/16, we are using the standard libarary part no. MB85RS64, not the actual part no. MB85RS64V.
// So the only mod to the Hackscribble_Ferro library is changing max block size from 0x40 to 0x80.
// To create an instance of FRAM, we specify a name, the FRAM part number, and our SS pin number, i.e.:
//    Hackscribble_Ferro FRAM1(MB85RS64, PIN_FRAM1);
// Control buffer in each FRAM is first 128 bytes (address 0..127) reserved for any special purpose we want such as config info.
#include <SPI.h>
#include <Hackscribble_Ferro.h>
const unsigned int FRAM_CONTROL_BUF_SIZE = 128;  // This defaults to 64 bytes in the library, but we modified it
// FRAM1 stores the Route Reference, Park 1 Reference, and Park 2 Reference tables.
// FRAM1 control block (first 128 bytes):
// Address 0..2 (3 bytes)   = Version number month, date, year i.e. 07, 13, 16
// Address 3..6 (4 bytes)   = Last known position of each turnout.  Bit 0 = Normal, 1 = Reverse.
// Address 7..36 (30 bytes) = Last known train locations for train 1 thru 10: trainNum, blockNum, direction
byte               FRAM1ControlBuf[FRAM_CONTROL_BUF_SIZE];
unsigned int       FRAM1BufSize             =   0;  // We will retrieve this via a function call, but it better be 128!
unsigned long      FRAM1Bottom              =   0;  // Should be 128 (address 0..127)
unsigned long      FRAM1Top                 =   0;  // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)
const byte         FRAM1VERSION[3]          = {  9, 16, 17 };  // This must match the version date stored in the FRAM1 control block.
byte               FRAM1GotVersion[3]       = {  0,  0,  0 };  // This will hold the version retrieved from FRAM1, to confirm matches version above.
const unsigned int FRAM1_ROUTE_START        = 128;  // Start writing FRAM1 Route Reference table data at this memory address, the lowest available address
const byte         FRAM1_ROUTE_REC_LEN      =  77;  // Each element of the Route Reference table is 77 bytes long
#ifdef SINGLE_LEVEL
  const byte       FRAM1_ROUTE_RECS         =  32;  // Number of records in the basic Route Reference table, not including "reverse" routes
#else
  const byte       FRAM1_ROUTE_RECS         =  70;  // Number of records in the basic Route Reference table, not including "reverse" routes
#endif
const byte         FRAM1_RECS_PER_ROUTE     =  11;  // Max number of block/turnout records in a single route in the route table.
const unsigned int FRAM1_PARK1_START        = FRAM1_ROUTE_START + (FRAM1_ROUTE_REC_LEN * FRAM1_ROUTE_RECS);
const byte         FRAM1_PARK1_REC_LEN      = 118;
const byte         FRAM1_PARK1_RECS         =  19;
const byte         FRAM1_RECS_PER_PARK1     =  21;  // Max number of block/turnout records in a single Park 1 route in the route table.
const unsigned int FRAM1_PARK2_START        = FRAM1_PARK1_START + (FRAM1_PARK1_REC_LEN * FRAM1_PARK1_RECS);
const byte         FRAM1_PARK2_REC_LEN      =  43;
const byte         FRAM1_PARK2_RECS         =   4;
const byte         FRAM1_RECS_PER_PARK2     =   6;  // Max number of block/turnout records in a single Park 2 route in the route table.
Hackscribble_Ferro FRAM1(MB85RS64, PIN_FRAM1);   // Create the FRAM1 object!
// FRAM2 control block (first 128 bytes) contains no data - we don't need a version number because we don't have any initial data to read.
// FRAM2 stores the Delayed Action table.  Table is 12 bytes per record, perhaps 400 records => approx. 5K bytes.
// byte               FRAM2ControlBuf[FRAM_CONTROL_BUF_SIZE];
// unsigned int       FRAM2BufSize             =   0;  // We will retrieve this via a function call, but it better be 128!
// unsigned long      FRAM2Bottom              =   0;  // Should be 128 (address 0..127)
// unsigned long      FRAM2Top                 =   0;  // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)
// const byte         FRAM2VERSION[3]          = { mm, dd, yy };  // This must match the version date stored in the FRAM2 control block.
// byte               FRAM2GotVersion[3]       = {  0,  0,  0 };  // This will hold the version retrieved from FRAM2, to confirm matches version above.
// Hackscribble_Ferro FRAM2(MB85RS64, PIN_FRAM2);  // Create the FRAM2 object!
// Repeat above for FRAM3 if we need a third FRAM

// *** 8-CHAR ALPHANUMERIC DISPLAY: Rev: 02/04/17.  Adafruit 8-char alphanumeric display requirements.
// Important: #include "Adafruit_GFX.h" is contained in Adafruit_LEDBackpack.h, so we don't need an #include statement
// here, but we do need Adafruit_GFX.h to be in our library.
#include "Adafruit_LEDBackpack.h"   // Required for alphanumeric displays on control panel.
Adafruit_AlphaNum4 alpha4Left = Adafruit_AlphaNum4();    // Left-most 4 characters
Adafruit_AlphaNum4 alpha4Right = Adafruit_AlphaNum4();   // Right-most 4 characters
char alphaString[9] = "        ";  // Global array to hold strings sent to alpha display.  8 chars plus \0 newline.

// *** ROTARY ENCODER: Rev: 10/26/16.  Interrupt-driven rotary encoder turns and button press functions requirements.
// We do *NOT* need a rotary encoder library for this to work.
// Here is a cross reference of the Mega interrupt number to pin numbers.  This is handled automatically via the digitalPinToInterrupt() function:
//   Mega2560:    int.0  int.1  int.2  int.3  int.4  int.5
//   Digital Pin:     2      3     21     20     19     18
#define DIR_CCW 0x10    // decimal 16
#define DIR_CW 0x20     // decimal 32
// This global full-step state table emits a code at 00 only:
const unsigned char ttable[7][4] = {
  {0x0, 0x2, 0x4,  0x0}, {0x3, 0x0, 0x1, 0x10},
  {0x3, 0x2, 0x0,  0x0}, {0x3, 0x2, 0x1,  0x0},
  {0x6, 0x0, 0x4,  0x0}, {0x6, 0x5, 0x0, 0x20},
  {0x6, 0x5, 0x4,  0x0},
};
volatile unsigned char rotaryNewState = 0;      // Value will be 0, 16, or 32 depending on how rotary being turned
volatile unsigned char rotaryTurned   = false;  // Flag set by ISR so we know to process in loop()
volatile unsigned char rotaryPushed   = false;  // Flag set by ISR so we know to process in loop()

// *** MISC CONSTANTS AND GLOBALS: needed by A-OCC:
// true = any non-zero number
// false = 0

const byte TOTAL_SENSORS           =  52;
const byte TOTAL_BLOCKS            =  26;
const byte MAX_TRAINS              =   8;
const byte TRAIN_ID_NULL           =   0;  // Used for "no train."
const byte TRAIN_ID_STATIC         =  99;  // This train number is a static train.
const byte TRAIN_DONE_ID           = 100;  // Used when selecting "done" on rotary encoder
const byte MAX_BLOCKS_PER_TRAIN    =  12;  // Maximum number of occupied blocks FOR ANY ONE TRAIN in the Train Progress table, to dimension array.
const byte MAX_REGISTERED_BLOCKS   =  20;  // How many total blocks could be occupied (by trains 1..10, or by "static" trains) during registration?  To dim array.
const byte LED_DARK                =   0;  // LED off
const byte LED_WHITE_SOLID         =   1;  // White occupancy LED lit solid
const byte LED_WHITE_BLINKING      =   2;  // White occupancy sensor LED lit blinking (not used as of 11/16)
const byte LED_RED_SOLID           =   1;  // RGB block LED lit red solid
const byte LED_RED_BLINKING        =   2;  // RGB block LED lit red blinking
const byte LED_BLUE_SOLID          =   3;  // RGB block LED lit blue solid
const byte LED_BLUE_BLINKING       =   4;  // RGB block LED lit blue blinking
const unsigned long LED_FLASH_MS   = 500;  // Toggle "conflicted" LEDs every 1/2 second
const unsigned long LED_REFRESH_MS = 100;  // Refresh LEDs on control panel every 100ms, just so it isn't constant
byte totalTrainsReceived = 0;              // This keeps track of how many trains we prompt operator about during registration.  Usually = MAX_TRAINS.
// byte totalPromptsReceived = 0;  not needed? // totalPromptsReceived will indicate the number of *active* elements in the registrationInput[] table.

// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// *** ROUTE REFERENCE TABLES...
// A-OCC needs route information to track reserved (and occupied) blocks when running in AUTO and PARK modes.

struct routeReference {                 // Each element is 77 bytes, 26 or 70 records (single or double level)
  byte routeNum;
  char originSiding[5];
  byte originTown;
  char destSiding[5];
  byte destTown;
  byte maxTrainLen;                     // In inches, typically the destination siding length
  char restrictions[6];                 // Possible future use
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate
  char route[FRAM1_RECS_PER_ROUTE][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_ROUTE per route, each with 4 chars (plus \0)
};
routeReference routeElement;            // Use this to hold individual elements when retrieved

struct park1Reference {                 // Each element is 118 bytes, total of 19 records
  byte routeNum;
  char originSiding[5];
  char destSiding[5];
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate
  char route[FRAM1_RECS_PER_PARK1][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_PARK1 per route, each with 4 chars (plus \0)
};
park1Reference park1Element;            // Use this to hold individual elements when retrieved

struct park2Reference {                 // Each record 43 bytes long, total of just 4 records
  byte routeNum;
  char originSiding[5];
  char destSiding[5];
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate
  char route[FRAM1_RECS_PER_PARK2][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_PARK2 per route, each with 4 chars (plus \0)
};
park2Reference park2Element;  // Use this to hold individual elements when retrieved

// BLOCK RESERVATION TABLE...
// Rev: 09/06/17.
// Used only in A-MAS and A-LEG as of 01/21/17; possibly also A-OCC, so it could illuminate reserved blocks as well as occupied?
// Can also add constant fields that indicate incoming speed L/M/H: EB from W, and WB from E, if useful for decelleration etc.



struct blockReservationStruct {
  byte reservedForTrain;     // Variable: 0 = unreserved, TRAIN_STATIC = permanently reserved, 1..8 = train number
  char whichDirection;       // Variable: [E|W] If reserved, Eastbound or Westbound (may not be needed)
  byte eEntrywExitSensor;    // Constant: 1..52.  Eastbound Entry sensor = Westbound Exit sensor
  byte wEntryeExitSensor;    // Constant: 1..52.  Westbound Entry sensor = Eastbound Exit sensor
  unsigned int blockLen;     // Constant: Length in mm.  Only used for siding blocks.  Used for slowing-down calcs.
  char blockSpeed;           // Constant: [L|M|H] Low, Medium, or High speed.
  char sidingType;           // Constant: [N|D|S] Not a siding, Double-ended siding, or Single-ended siding
  char forbidden;            // Constant: [P|F|L] Passenger/Freight/Local.  Not used yet, but should correspond to Train Data table value.
  char tunnel;               // Constant: [Y|N] Y if block is in a tunnel, else N
  char curvature;            // Not used
  char grade;                // Not used
};

// Populate the Block Reservation table with known constants, and init variable fields.
// Rev: 09/06/17.
// BE SURE TO ALSO UPDATE IN OTHER MODULES THAT USE THIS DATA IF CHANGED.
// This is a READ-WRITE table, total size around 312 bytes.  Could be stored in FRAM 3.
// Note that block number 1..26 corresponds with element 0..25, not stored in table.
blockReservationStruct blockReservation[TOTAL_BLOCKS] = {   // 26 blocks, offset 0 thru 25
  {0,' ', 1, 2,4548,'H','D',' ','N',' ',' '},
  {0,' ', 3, 4,3175,'H','D',' ','N',' ',' '},
  {0,' ', 5, 6,3200,'H','D',' ','N',' ',' '},
  {0,' ', 7, 8,5182,'L','D',' ','N',' ',' '},
  {0,' ', 9,10,3912,'M','D',' ','N',' ',' '},
  {0,' ',11,12,3658,'M','D',' ','N',' ',' '},
  {0,' ',32,31,   0,'M','N',' ','Y',' ',' '},
  {0,' ',34,33,   0,'M','N',' ','N',' ',' '},
  {0,' ',36,35,   0,'H','N',' ','N',' ',' '},
  {0,' ',38,37,   0,'H','N',' ','N',' ',' '},
  {0,' ',40,39,   0,'M','N',' ','N',' ',' '},
  {0,' ',41,42,   0,'M','N',' ','Y',' ',' '},
  {0,' ',17,18,9601,'M','D',' ','Y',' ',' '},
  {0,' ',13,14,3683,'M','D',' ','N',' ',' '},
  {0,' ',15,16,4039,'M','D',' ','N',' ',' '},
  {0,' ',43,44,   0,'M','N',' ','Y',' ',' '},
  {0,' ',45,46,   0,'H','D',' ','N',' ',' '},
  {0,' ',47,48,   0,'M','D',' ','N',' ',' '},
  {0,' ',19,20,3480,'M','D',' ','N',' ',' '},
  {0,' ',21,22,3632,'M','D',' ','N',' ',' '},
  {0,' ',51,52,   0,'M','N',' ','Y',' ',' '},
  {0,' ',50,49,   0,'L','N',' ','N',' ',' '},
  {0,' ',23,24,   0,'L','L',' ','N',' ',' '},
  {0,' ',25,26,   0,'L','L',' ','N',' ',' '},
  {0,' ',27,28,   0,'L','L',' ','N',' ',' '},
  {0,' ',29,30,   0,'L','L',' ','N',' ',' '}
};

// BLOCK-SENSOR CROSS-REFERENCE TABLE...
// 09/26/17: ENTRY AND EXIT SENSOR NUMBER FOR GIVEN BLOCK NUMBER.  Offset by 1 (0..25 for 26 blocks).
// There will be 26 records for blocks 1..26, implied by array offset (0..25) + 1
// If memory gets tight, this could EASILY be moved to FRAM.
struct blockSensorStruct {
  byte eastEntryWestExit;       // Sensor #1 thru 52
  byte westEntryEastExit;
};
blockSensorStruct blockSensor[TOTAL_BLOCKS] = { 1,2,3,4,5,6,7,8,9,10,11,12,32,31,34,33,36,35,38,37,40,39,41,42,17,18,13,14,15,16,43,44,45,46,47,48,19,20,21,22,51,52,50,49,23,24,25,26,27,28,29,30 };

// SENSOR-BLOCK CROSS-REFERENCE TABLE...
// 09/26/17: WHICH BLOCK AND WHICH END IS A GIVEN SENSOR NUMBER.  Offset by 1 (0..51 for 52 sensors).
// There will be 52 records for sensors 1..52, implied by array offset (0..51) + 1
struct sensorBlockStruct {
  byte whichBlock;    // 1..26
  char whichEnd;      // E or W
};
sensorBlockStruct sensorBlock[TOTAL_SENSORS] = { 1,'W',1,'E',2,'W',2,'E',3,'W',3,'E',4,'W',4,'E',5,'W',5,'E',6,'W',6,'E',14,'W',14,'E',15,'W',15,'E',13,'W',13,'E',19,'W',19,'E',20,'W',20,'E',23,'W',23,'E',24,'W',24,'E',25,'W',25,'E',26,'W',26,'E', 7,'E', 7,'W', 8,'E', 8,'W', 9,'E', 9,'W',10,'E',10,'W',11,'E',11,'W',12,'W',12,'E',16,'W',16,'E',17,'W',17,'E',18,'W',18,'E',22,'E',22,'W',21,'W',21,'E' };

// SENSOR LED STATUS.  TELLS US HOW EACH WHITE OCCUPANCY SENSOR LED *SHOULD* BE LIT: ON, OFF, BLINKING.
// Rev: 09/26/17.
// We will use this on our final step when we populate/paint the control panel LEDs.
// Record corresponds to (sensor number - 1) = (0..51) for sensors 1..52
// Centipede pin 64 corresponds with white LED occupancy sensor 1
// Centipede pin 127 corresponds with white LED occupancy sensor 64
// const byte LED_DARK = 0;                  // LED off
// const byte LED_WHITE_SOLID = 1;           // White occupancy LED lit solid
// const byte LED_WHITE_BLINKING = 2;        // White occupancy sensor LED lit blinking (not used as of 11/16)
// The following zeroes should really be {LED_DARK, LED_DARK, LED_DARK, ... } but just use zeroes here for convenience

byte sensorLEDStatus[TOTAL_SENSORS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // 52 zeroes

// BLOCK LED STATUS.  TELLS US HOW EACH RED/BLUE BLOCK STATUS LED *SHOULD* BE LIT: RED/BLUE ON/OFF/BLINKING.
// Rev: 10/29/16.
// The paintControlPanel() function can use this when it populates/paints the control panel LEDs.  Nobody else needs to worry about it.
// This can be LOCAL to paintControlPanel().  Doesn't even need to be static as we will re-build the whole thing each time we run the function.
// Record corresponds to (block number - 1) = (0..25) for blocks 1..26 as of 10/29/16
// Centipede shift register pins don't correspond directly since there are TWO Centipede pins for each two-color LED.
// const byte LED_DARK = 0;                  // LED off
// const byte LED_RED_SOLID = 1;             // RGB block LED lit red solid
// const byte LED_RED_BLINKING = 2;          // RGB block LED lit red blinking
// const byte LED_BLUE_SOLID = 3;            // RGB block LED lit blue solid
// const byte LED_BLUE_BLINKING = 4;         // RGB block LED lit blue blinking

// WHITE LED PIN NUMBER FOR GIVEN SENSOR NUMBER.  Cross-reference array that gives us a Centipede pin number 0..52 for a corresponding
// Rev: 11/04/16.
// WHITE LED for a given sensor number 1..53.  Since we wired sensor #x to Centipede pin #x, we don't need an actual cross-reference table.
// However, don't overlook that Centipede pin numbers start at zero, and sensor numbers start at 1.

// RED/BLUE LED PIN NUMBER FOR GIVEN BLOCK NUMBER.  Cross-reference array that gives us a Centipede pin number 0..63 for a corresponding
// Rev: 10/29/16.
// RED or BLUE LED for a given block number - 1 (i.e. Centipede pin number of block 1 is pin 0.)  This is just how we chose to wire things together.
// CENTIPEDE #1, Address 0: RED/BLUE BLOCK INDICATOR LEDS
// Centipede pin 0 corresponds with block 1 blue
// Centipede pin 15 corresponds with block 16 blue
// Centipede pin 16 corresponds with block 1 red
// Centipede pin 31 corresponds with block 16 red
// Centipede pin 32 corresponds with block 17 blue
// Centipede pin 47 corresponds with block 32 blue   // though there are only 26
// Centipede pin 48 corresponds with block 17 red
// Centipede pin 63 corresponds with block 32 red  // though there are only 26
const byte LEDBlueBlockPin[TOTAL_BLOCKS] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,32,33,34,35,36,37,38,39,40,41};  // 26 bytes 0..25 = Blocks 1..26 (blue)
const byte LEDRedBlockPin[TOTAL_BLOCKS]  = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,48,49,50,51,52,53,54,55,56,57};  // 26 bytes 0..25 = Blocks 1..26 (red)

// For the rotary encoder and a/n display, we'll be sending an array of 8-character choices for the operator to select from.
// When the click the rotary encoder, it will return that array index as their selection.  This is for things like SMOKE Y/N,
// and to select an engine from a list of them during registration.
// We dimension MAX_ROTARY_PROMPTS to MAX_TRAINS + 2 to accommodate all train names, plus 'STATIC' and 'DONE'.  Other prompts will use only 2 or 3
// elements.  This is used for both various Y/N pre-registration prompts, as well as for registering trains.
const byte MAX_ROTARY_PROMPTS = MAX_TRAINS + 2;  // Arbitrary, must be at least MAX_TRAINS + 2, to allow for every train plus Static and Done
byte numRotaryPrompts         = 0;               // Used when we call the function to prompt on a/n display with rotary encoder
struct rotaryPromptStruct {
  byte promptNum;      // Used when registering trains to keep track of the train number (not necessarily same as index into array); not used for other prompts but could be.
  char promptText[9];  // 8-char a/n description for rotary encoder prompting, such as train name, or prompt text, plus null string terminator \0.
  byte lastBlock;      // "Real" block number (1..n) that this train was known to occupy; used as default when registering a train for a given block
};
rotaryPromptStruct rotaryPrompt[MAX_ROTARY_PROMPTS];  // The most prompts I can envision would be every train plus Static and Done

// DESCRIPTION OF CIRCULAR BUFFERS Rev: 9/5/17.
// We use CLOCKWISE circular buffers, and tracks HEAD, TAIL, and COUNT.
// HEAD always points to the UNUSED cell where the next new element will be inserted (enqueued), UNLESS the queue is full,
// in which case head points to tail and new data cannot be added until the tail data is dequeued.
// TAIL always points to the last active element, UNLESS the queue is empty, in which case it points to garbage.
// COUNT is the total number of active elements, which can range from zero to the size of the buffer.
// Initialize head, tail, and count to zero.
// To enqueue an element, if array is not full, insert data at HEAD, then increment both HEAD and COUNT.
// To dequeue an element, if array is not empty, read data at TAIL, then increment TAIL and decrement COUNT.
// Always incrment HEAD and TAIL pointers using MODULO addition, based on the size of the array.
// Note that HEAD == TAIL *both* when the buffer is empty and when full, so we use COUNT as the test for full/empty status.

// TRAIN PROGRESS STRUCTURE Rev: 9/5/17.
// An array of a structures.  Each structure represents the active route of a single train, and contains a circular buffer head, tail,
// and count (number of active records), as well as an array of about a dozen blocks, which includes Block Number, Entry Sensor, and
// Exit Sensor numbers.  There is one element of the structure array for each train -- so about 8 structure array elements.
// Each record represents a reserved block that is part of the train's current reserved route.
// New records are added when a train is first registered (just a single block) and when new routes are added (multiple blocks.)
// Old records are "cleared" as the train progresses along its route, based on exit sensors clearing.
// The blocks in the Block Reservation table are closely tied to these records -- when new records are added to Train Progress, the
// corresponding blocks are reserved in the Block Reservation table; and when Train Progress exit sensor records are cleared (i.e. when
// a block is released/dequeued), the corresponding blocks are "un-reserved" in the Block Reservation table.
// Similarly, every active record in the Train Progress table must have exactly one "reserved" record in the Block Reservation table.
// We might want to run a periodic function that confirms that the number of active Train Progress records (sum of the "length" for all trains)
// is equal to the number of records flagged as "Reserved" in the Block Reservation table.  Fatal error if they are ever not the same.
// When the table is initialized (with no train), head == tail == 0, and length == 0.
// When a new train is first established, it will have just a single record where head == tail == 0, and length = 1.
// As a route is added, records will be added at the head and then head and length will be incremented.
// As the train clears blocks, they will be "un-reserved" and the tail will be incremented and the length will be decrimented.
// trainProgress does not keep track of the actual location of the train; only that the train has those blocks reserved, and
// the train is guaranteed to be somewhere within that route -- not necessarily at the head or the tail.
// The code will watch the sensors being tripped and cleared, and infer the location of each train to be at, or just beyond (if the train
// is moving), the tripped sensor which is closest to the head.
// When we receive a sensor status change, we must be CERTAIN that we find it in the Train Progress talbe, otherwise it's a fatal error!

// DEBUG NOTE: It will be easy to provide debug code to the serial monitor that shows a "picture" of an entire array -- each reserved
// block, including entry and exit sensor, and the status (tripped or cleared), updated each time something happens.
// Perhaps we can show a list of sensos tripped and cleared separately from the linear "map" of the route.
// (In this context, "route" does not necessarily correspond exactly with a specific "route table" route, but rather all or a portion of
// one or two specific routes (the portion(s) reserved for this train,) or even just a single block when a train is first registered.

// WITH REGARDS TO A-OCC: Painting the red/blue and white occupancy sensor LEDs:  If we update the train progress table every time a sensor
// status changes, we will have enough information to paint the control panel red/blue and white LEDs.
// We will pass a parm to the paint routine "train number selected" and paint that red, if any.

struct trainProgressStruct {
  byte head;                               //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Next array element to be written
  byte tail;                               //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Next array element to be removed
  byte count;                              //         = 0..(MAX_BLOCKS_PER_TRAIN - 1) Number of active elements (reserved blocks) for this train.
  byte blockNum[MAX_BLOCKS_PER_TRAIN];     // [0..11] = 1..26
  byte entrySensor[MAX_BLOCKS_PER_TRAIN];  // [0..11] = 1..52 (or 53)
  byte exitSensor[MAX_BLOCKS_PER_TRAIN];   // [0..11] = 1..52 (or 53)
};
trainProgressStruct trainProgress[MAX_TRAINS];  // Create trainProgress[0..MAX_TRAINS - 1]; thus, trainNum 1..MAX_TRAINS is always 1 more than the index.

// REGISTRATION INPUT TABLE Rev: 11/5/16.
// List of train info and default location received from A-MAS to use for registration.  Direction implied by sensor.
struct regInputStruct {
  bool registered;    // true or false
  byte trainNum;      // 1..MAX_TRAINS, must be sequential
  char trainName[9];  // 8-char a/n description for rotary encoder prompting (0 thru 7, element 8 is newline)
  byte lastBlock;     // 1..32 or 0 default block number sent by A-MAS for prompting
  byte newBlock;      // 1..32 the block that the user tells us that the train is in, or 0 if the train is not to be assigned.  Returned to A-MAS.
  char newDirection;  // 'E' or 'W' inferred by A-OCC based on which end of the block has a tripped sensor.  Returned to A-MAS if train is assigned.
};
regInputStruct registrationInput[MAX_TRAINS];

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485  up to 115200
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // Set all chips on Centipede shift register to OUTPUT, high (i.e. turn off all LEDs)
  initializePinIO();                    // Initialize all of the I/O pins
  initializeLCDDisplay();               // Initialize the Digole 20 x 04 LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);
  initializeFRAM1AndGetControlBlock();  // Check FRAM1 version, and get control block
  sprintf(lcdString, "FRAM1 Rev. %02i/%02i/%02i", FRAM1GotVersion[0], FRAM1GotVersion[1], FRAM1GotVersion[2]);  // FRAM1 version no. on LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);
  //  initializeFRAM2();
  // Call trainProgressInit, not:
//   initializeTrainProgress();            // Clears out both trainProgress[] and trinProgressPointer[]
  // 8-char alphanumeric display (actually, two 4-char displays)
  alpha4Left.begin(0x70);               // Pass in the address
  alpha4Left.clear();
  alpha4Right.begin(0x71);              // Pass in the address
  alpha4Right.clear();
  sprintf(alphaString, "        ");
  sendToAlpha(alphaString);             // Send 8 blanks to the a/n display so we don't see garbage at startup

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // We need to watch for the following RS485 messages:
  //   In ANY mode:
  //     From A-MAS to ALL broadcasting change of mode and state (i.e. Register Mode, Running)
  //     From A-SNS to A-MAS reporting a sensor status change.
  //   In REGISTER mode, RUNNING state:  IMPORTANT: Sensor status MUST NOT change during Registration, all trains must be stopped!
  //       We can add logic to check before-and-after status of every sensor, but for now we'll just assume that we don't want to crash the program.
  //     From A-MAS to A-OCC (us) requesting a reply from the operator via a/n display.  i.e. SMOKE Y/N.  Results in one record reply to A-MAS.
  //     From A-MAS to A-OCC (us) requesting operator registration data.  This will kick off a series of RS485 messages going back to A-MAS.
  //   In AUTO or PARK mode, RUNNING or STOPPING state:
  //     From A-MAS to A-LEG assigning a new route to a new train (we must track block status and reservations etc.) (will not occur if state = STOPPING)
  //     From A-MAS to ALL, changing state to STOPPING -- probably nothing special to do until another mode started.
  //     From A-MAS to ALL, changing state to STOPPED -- probably nothing to do until another mode started.  Maintain Train Progress in case we start Auto again.

  // IN ANY MODE, CHECK INCOMING RS485 MESSAGES...
  if (RS485GetMessage(RS485MsgIncoming)) {   // WE HAVE A NEW RS485 MESSAGE! (may or may not be relevant to us)

    // Need to reset the state and mode changed flags so we don't execute special code more than once...
    stateChanged = false;
    modeChanged = false;

    // ***********************************************************************************
    // ********** CHECK FOR MODE/STATE-CHANGE MESSAGE AND UPDATE AS APPROPRIATE **********
    // ***********************************************************************************
    // Entering MANUAL or POV mode, we might need to refresh the LEDs on the control panel, but might not be necessary...???
    // Entering REGISTER mode, we handle it all in a separate code block below, because.
    // Entering AUTO or PARK mode, we will already have initialized Train Progress and Block Reservation, when trains were Registered.
    //   So, not sure if there is anything specific to do at this moment.
    if (RS485fromMAStoALL_ModeMessage()) {  // If we just received a mode/state change message from A-MAS
      RS485UpdateModeState(&modeOld, &modeCurrent, &modeChanged, &stateOld, &stateCurrent, &stateChanged);
    }

    // CHECK FOR SENSOR-CHANGE MESSAGE AND HANDLE FOR ALL MODES...
    // In Manual or P.O.V. mode, we'll just update the sensor and block LEDs on the control panel, easy.
    // In Register mode, we'll trigger a Halt because sensors should never change during registratino.
    // In Auto or Park mode, we'll need to update the Train Progress table and THEN update the LEDs.
    if (RS485fromSNStoMAS_SensorMessage()) {
      byte sensorNum = RS485MsgIncoming[4];  // Will return a "real" sensor number i.e. non-zero, starting with sensor #1, if applicable
      byte sensorTripType = RS485MsgIncoming[5];   // 0 = cleared, 1 = tripped
      // What we do next depends if we are in Manual/POV mode, or Auto/Park mode...
      if ((modeCurrent == MODE_MANUAL) || (modeCurrent == MODE_POV)) {
        // If we are in Manual or POV mode, regardless of state, simply update the white LEDs to reflect current status
        sensorLEDStatus[sensorNum - 1] = sensorTripType;   // Will be 0 or 1, for off or on
      } else if (modeCurrent == MODE_REGISTER) {
        // If we are in Register mode and a sensor changes, that's a fatal error!
        sprintf(lcdString, "%.20s", "SNS CHG DURING REG!");
        sendToLCD(lcdString);
        Serial.print(lcdString);
        endWithFlashingLED(3);
      } else if ((modeCurrent == MODE_AUTO) || (modeCurrent == MODE_PARK)) {
        // Update the Train Progress table and the Block Reservation table, as appropriate.
        // Scan the Train Progress table to find the sensor, and that will identify the Train and the Block numbers.
        // If this is the ENTRY SENSOR TRIP to a block on the route, then we need to change the status of the block from Reserved to Occupied,
        //   so that the Red/Blue Block Occupancy LEDs can be updated appropriately.
        // If this is the ENTRY SENSOR CLEAR to a block on the route, then we don't need to do anything.
        // If this is the EXIT SENSOR TRIP to a block on the route, then we don't need to do anything.
        // If this is the EXIT SENSOR CLEAR to a block on the route, then we need to change the status of the block from Occupied to Unreserved,
        //   *and* update the Train Progress table to remove that block and increment the Tail pointer and decrement the Length.
        sensorLEDStatus[sensorNum - 1] = sensorTripType;   // Will be 0 or 1, for off or on
      }
    }      // End of handling the RS485 sensor changed message.



    

    // Lots of special handling here, in the event that the operator/A-MAS just changed our mode and/or state.
    // We will rely on A-MAS to ensure that all mode transitions we might see are legal, so we don't need special code to check that.
    // For example, you can't transition from REGISTER RUNNING to REGISTER STOPPED without first announcing REGISTER STOPPING.
    // Similarly, you can't transition from MANUAL to AUTO without first going through REGISTER mode.  Things like that.
    // And A-MAS will not allow the user to stop registration -- only A-MAS can stop it, after getting the okay from A-OCC.
    // We will also assume that the operator will follow the rules, such as not change the location of any trains during registration,
    // so we will not look for any sensor or mode changes during registration.

    // In ANY MODE, if State = STOPPED, Clear the display but retain sensor states, track sensor changes, and simply wait for Mode/State to change.

    // Mode = MANUAL or POV, State = RUNNING: Paint panel first time through, then watch sensor changes and paint panel.
    // Mode = MANUAL or POV, State = STOPPING: Will never happen.

    // Mode = REGISTER, State = RUNNING: We have a single block of code, below, that does everything.
    // Mode = REGISTER, State = STOPPING: Will never happen.

    // Mode = AUTO, State = RUNNING: Watch for sensor changes and new routes, update train progress, paint display.
    // Mode = AUTO, State = STOPPING: Same as RUNNING.

    // Mode = PARK, State = RUNNING: Maybe just treat this as MANUAL.  Watch sensors and paint panel but no reserved routes.
    // Mode = PARK, State = STOPPING: Same as RUNNING.

    // Check if we have a new state = STOPPED.  Note that we don't care what the mode is if our state is STOPPED.
    // Just clear the panel, update the "stateChanged" variable, and do nothing until we get a new mode that is RUNNING (or STOPPING)

    if ((stateChanged) && (stateCurrent == STATE_STOPPED)) {  // Any special handling for when any mode has just been STOPPED

      //  paintControlPanel(0); // Not needed here because will be done at end of loop()

    } 

    if (modeChanged || stateChanged) {    // Either mode or state changed, so do some special handling

      // If the mode changed, and we are now (newly) in REGISTRATION, RUNNING, then we need to (re)init the train data.
      if ((modeCurrent == MODE_REGISTER) && (stateCurrent == STATE_RUNNING)) {
        totalTrainsReceived = 0;             // Counts the number of elements in the registrationInput[] table.
        initializeRegInput();                // Clears data from registrationInput[]
        initializeBlockReservationTable();   // INITIALIZE THE BLOCK RESERVATION TABLE to set all blocks as unreserved.
// Call trainProgressInit()
// Not:        initializeTrainProgress();           // Clears out both trainProgress[] and trinProgressPointer[]
        paintControlPanel(TRAIN_ID_STATIC);                // Passing 99 (TRAIN_STATIC) in this mode means turn off all LEDs
      }
    }   // end of "if either the mode or the state ust changed"

    // See if the RS485 message is a request from A-MAS to us (A-OCC) to request REGISTRATION QUERY data from operator...
    // This could include Smoke Yes/No, Startup Fast/Slow, or Use P.A. System Yes/No.  Allow for possibly multiple selections,
    // not just two (yes/no, fast/slow, etc.)  Won't use any yet, but could add later if needed for something.
    // A-MAS can include as many as MAX_ROTARY_PROMPTS prompts for each question.
    // This will only occur in Registration mode, so no need to check that.
    // This is NOT the block of code that requests train locations; that code follows below this block.
    if (RS485fromMAStoOCC_Question()) {  // If A-MAS is asking us to prompt the operator for a question such as Smoke Y/N, etc.
      initRotaryPrompts();  // Clear out the entire rotaryPrompt[] array
      numRotaryPrompts = 1; // We know we have at least one -- THIS one!
      do {
        if (numRotaryPrompts > MAX_ROTARY_PROMPTS) {  // A-MAS sent more prompts than our array can handle
          sprintf(lcdString, "%.20s", "TOO MANY PROMPTS!");
          sendToLCD(lcdString);
          Serial.print(lcdString);
          endWithFlashingLED(3);
        }
        memcpy(rotaryPrompt[numRotaryPrompts - 1].promptText, RS485MsgIncoming + 4, 8);
        if (RS485MsgIncoming[12] == 'N') {   // If this is not the last query record coming from A-MAS at this time
          do { } while (RS485GetMessage(RS485MsgIncoming) == false); // Get the next packet, guaranteed by A-MAS to be another query record
          numRotaryPrompts++;  // Increment the number of prompts we're going to prompt for 1..?
        } else {   // There are no more prompt text records so start prompting (we assume incoming buffer[12] = 'Y' = last record just sent)
          break;
        }
      } while(true);
      byte promptNum = getRotaryResponse(0);  // Returns user selected promptNum to 0..numRotaryPrompts - 1
      // Now send the prompt number "promptNum" (0..n) back to A-MAS...  First prompt sent by A-MAS is prompt 0, and so on.
      Serial.println("Sending RS485 to A-MAS with results of query...");
      RS485MsgOutgoing[RS485_LEN_OFFSET] = 6;   // Byte 0 is length of message
      RS485MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_MAS;  // Byte 1 is "to".
      RS485MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_OCC;  // Byte 2 is "from".
      RS485MsgOutgoing[3] = 'R';   // Byte 3 = R for Reply
      RS485MsgOutgoing[4] = promptNum;
      RS485MsgOutgoing[5] = calcChecksumCRC8(RS485MsgOutgoing, 5); 
      RS485SendMessage(RS485MsgOutgoing);
      sprintf(alphaString, "        ");
      sendToAlpha(alphaString);            // Clear response from a/n display
    }   // end of "if it's an RS485 Question message to A-OCC"

    // See if the RS485 message is a request from A-MAS to us (A-OCC) to request REGISTRATION data from operator...
    // This will only occur in Registration mode, so no need to check that.
    if (RS485fromMAStoOCC_Registration()) {
      // Here, we will receive a prompt data record (name, number) for an engine that A-MAS wants us to include
      // on the registration prompt list.  Each record will include a field that indicates whether or not this is
      // the LAST record that it is sending us (record type 'Y' for "Yes this is the last record"), or if we should
      // expect another (record type 'N' for "Not the last one I'm sending you.")
      // "Train number" will be sent sequentially starting with train #1 and finishing with however many trains are
      // defined in the Train Reference table accessed in A-MAS FRAM.
      totalTrainsReceived = 1;  // We know we have at least one train to prompt for (this one) but maybe more coming...
      do {
        // We'd better get these records in sequence starting with Train #1.
        if (RS485MsgIncoming[4] != totalTrainsReceived) {   // Out of sync error!
          sprintf(lcdString, "%.20s", "REG TRAIN SYNC ERR!");
          sendToLCD(lcdString);
          Serial.print(lcdString);
          endWithFlashingLED(3);
        }
        registrationInput[totalTrainsReceived - 1].registered = false;
        registrationInput[totalTrainsReceived - 1].trainNum = totalTrainsReceived;
        // The 8-char a/n description of the train is stored in elements 5 thru 12.
        memcpy(registrationInput[totalTrainsReceived - 1].trainName, RS485MsgIncoming + 5, 8);
        registrationInput[totalTrainsReceived - 1].lastBlock = RS485MsgIncoming[13];     // Default block number for this train
        registrationInput[totalTrainsReceived - 1].newBlock = 0;           // Not sure if we will use this
        registrationInput[totalTrainsReceived - 1].newDirection = ' ';     // Not sure if we will use this
        Serial.print("Received train: '"); Serial.print(registrationInput[totalTrainsReceived - 1].trainName); Serial.println("'");

        if (RS485MsgIncoming[14] == 'N') {   // If this is not the last train record coming from A-MAS at this time
          do { } while (RS485GetMessage(RS485MsgIncoming) == false); // Get the next packet, guaranteed by A-MAS to be another train info record
          totalTrainsReceived++;  // Increment the number of trains we're going to prompt for 1..?
        } else {   // There are no more train data request records so start prompting.
          break;
        }
      } while(true);
      Serial.print("Received info on "); Serial.print(totalTrainsReceived); Serial.println(" trains.");

      // Now we have the default train data from A-MAS.  Use it, along with known occupied sensors, to prompt operator for
      // the train ID associated with every occupied block -- either a real train number, or STATIC.
      // Use the list of registrationInput that A-MAS sent us as the list of choices, along with a "STATIC" train.
      // Start with a list of all trains, STATIC, and DONE.  Remove trains from the list of choices as each is assigned.  This is done by
      // changing the registrationInput[train_number].registered flag from false to true.
      // Operator selecting DONE means all unassigned occupied blocks are static.

      // Control panel LEDs should be painted as follows:
      //   The block that is being prompted for a train ID should have a FLASHING RED LED lit.
      //   All other LEDs, including white sensor LEDs, should remain dark during registration.

      // Send the registrationInput[] table to the rotary+a/n display and get user response for each occupied block.
      // AS EACH TRAIN IS IDENTIFIED, we'll send an RS485 message to A-MAS (and A-LEG will hear) so it can create a Train Progress entry,
      // and A-LEG can start up the train.  It's important that A-MAS doesn't send the commands to A-LEG because A-MAS needs to be listening
      // for further trains, or "done", from A-OCC.
      // So anyone else who needs to maintain a Train Progress table (and perhaps block reservation etc.) should simply watch for these
      // registration records from A-OCC to A-MAS, and A-OCC will also create new Train Progress records at this time.
      // Every occupied block will either be identified as a "real" train number, 99 for Static, or Done to assume this block and all remaining
      // undefined blocks are static.  
      // Repeat until all unoccupied blocks have been assigned to something, OR all trains have been assigned, OR operator selects DONE.

      // First set the block reservation table to default every occupied block reserved for train #TRAIN_STATIC, and all others train zero (unreserved.)
      // All have already been defaulted to zero, so just change if either sensor is tripped.
      for (byte sensor = 0; sensor < TOTAL_SENSORS; sensor++) {
        if (sensorLEDStatus[sensor] == 1) {    // if it's occupied (lit solid or blinking)
          blockReservation[sensorBlock[sensor].whichBlock - 1].reservedForTrain = TRAIN_ID_STATIC;
          blockReservation[sensorBlock[sensor].whichBlock - 1].whichDirection = sensorBlock[sensor].whichEnd;  // Which end is same as which direction is it facing
        }
      }

      // Now get the train number of any blocks that are occupied by "real" trains...
      for (byte block = 0; block < TOTAL_BLOCKS; block++) {       // For every block on the layout
        if (blockReservation[block].reservedForTrain == TRAIN_ID_STATIC) {     // Something is there, is it a real train?
          // Paint the control panel to have *only* this block lit, blinking.  All other white and red/blue LEDs should be off.
          paintControlPanel(block + 1);  // Pass it the "real" block number i.e starting at block 1
          // Call a function that is passed the registrationInput[] table, and uses only not-yet-registered trains as a set of choices
          // displayed on the a/n display.  We will update registrationInput[].registered, newBlock, and newDirection.
          byte trainIdentified = promptOperatorForTrain(block + 1);   // Given "real" block no., returns real no. of train identified [1..MAX_TRAINS|TRAIN_STATIC|TRAIN_DONE]
          if (trainIdentified == TRAIN_ID_STATIC) {
            // Nothing to do, just move on to the next one
          } else if (trainIdentified == TRAIN_DONE_ID) {                     // CHANGE THIS TO A CONSTANT FOR DONE i.e 100 (or zero???)
            break;    // Will this get us out of the "for" loop or just the "if" block?
          } else {    // Must be a "real" train number 1..MAX_TRAINS
            registrationInput[trainIdentified - 1].registered = true;
            registrationInput[trainIdentified - 1].newBlock = block + 1;  // The "real" block number, starting at 1
            registrationInput[trainIdentified - 1].newDirection = blockReservation[block - 1].whichDirection;
            sprintf(lcdString, "Start train %2d", trainIdentified);
            sendToLCD(lcdString);
            sprintf(lcdString, "Block %2d %c", block + 1, blockReservation[block].whichDirection);
            sendToLCD(lcdString);
            // Send an RS485 message to A-MAS that this train is registered for this block and direction
            RS485MsgOutgoing[RS485_LEN_OFFSET] = 9;   // Byte 0 is length of message
            RS485MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_MAS;  // Byte 1 is "to".
            RS485MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_OCC;  // Byte 2 is "from".
            RS485MsgOutgoing[3] = 'T';   // Byte 3 = T for Train registration data
            RS485MsgOutgoing[4] = trainIdentified;  // Train number - a real train!
            RS485MsgOutgoing[5] = block + 1;  // Real block number of the train
            RS485MsgOutgoing[6] = blockReservation[block].whichDirection;  // Direction of the train
            RS485MsgOutgoing[7] = 'N';  // Not done yet!
            RS485MsgOutgoing[8] = calcChecksumCRC8(RS485MsgOutgoing, 8); 
            RS485SendMessage(RS485MsgOutgoing);
          }
        }  // end of "If this block is reserved for Static"
      }  // End of for loop: get here when every block has been queried, or every train has been identified, or operator selects Done...
      // When we get here, then one of the following is true:
      // 1. We ran through every block, and the operator idendified either a train # or Static for every occupied block, so we're done; or
      // 2. We identified every train in the registrationInput table (promptOperatorForTrain would return trainIdentified as 0; same behavior as Done; or
      // 3. The operator selected Done.
      // So go ahead and send a final RS485 message to A-MAS indicating that we are done registering trains...
      RS485MsgOutgoing[RS485_LEN_OFFSET] = 9;   // Byte 0 is length of message
      RS485MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_MAS;  // Byte 1 is "to".
      RS485MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_OCC;  // Byte 2 is "from".
      RS485MsgOutgoing[3] = 'T';   // Byte 3 = T for Train registration data
      RS485MsgOutgoing[4] = 0;     // We are done, so train number = 0 = n/a
      RS485MsgOutgoing[5] = 0;     // We are done, so block number = 0 = n/a
      RS485MsgOutgoing[6] = ' ';   // We are done, so direction = ' ' = n/a
      RS485MsgOutgoing[7] = 'Y';   // We are done, so "done" = 'Y'
      RS485MsgOutgoing[8] = calcChecksumCRC8(RS485MsgOutgoing, 8); 
      RS485SendMessage(RS485MsgOutgoing);

      // Last thing to do is to clear the 8-char a/n display...
      sprintf(alphaString, "        ");
      sendToAlpha(alphaString);      // Send 8 blanks to the a/n display

      // ALL DONE WITH REGISTRATION!
    }   // end of "if it's an RS485 Registration message to A-OCC"


// LEFT OFF HERE 2/20/17

    // See if the RS485 message is a new route assignment from A-MAS to A-LEG.
    // This will only occur when in AUTO or PARK mode, so we don't even need to confirm mode
    if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
      if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
        if (RS485MsgIncoming[3] == 'R') {  // It's a new Route assignment (for a particular train) from A-MAS to A-LEG.
          // Also check for Park1 and Park2...







        }
      }
    }      // End of handling the RS485 new route assignment from A-MAS to A-LEG

  }  // END OF "IF WE HAVE A NEW RS485 INCOMING MESSAGE"

  ////******** WE MAY WANT TO USE SOME OF THE FOLLOWING FOR RETRIEVING ROUTES SO KEEP IT FOR NOW...***************


  /*

      // See if A-MAS is sending A-LEG a "new route" command -- in which case we'll need to update some of our data as well...

      if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
        // WE JUST ASSUME THAT ANY MESSAGE TO A-LEG MUST BE FROM A-MAS, AND IF IT'S A T, 1, or 2, THEN WE MUST BE IN AUTO OR PARK WANTING TO ASSIGN A NEW ROUTE.
        if (RS485MsgIncoming[3] == 'T') {    // rouTe command, so create a bunch of turnout commands...OLD CODE FROM A-SWT...CHANGE FOR A-OCC...
          byte routeNum = RS485MsgIncoming[4];
          sprintf(lcdString, "Route: %2i", RS485MsgIncoming[4]);
          sendToLCD(lcdString);
          Serial.println(lcdString);
          // Retrieve route number "routeNum" from Route Reference table (FRAM1) and create a new record in the
          // turnout command buffer for every turnout in the route...
          unsigned long FRAM1Address = FRAM1_ROUTE_START + ((routeNum - 1) * FRAM1_ROUTE_REC_LEN); // Rec # vs Route # offset by 1
          byte b[FRAM1_ROUTE_REC_LEN];  // create a byte array to hold one Route Reference record
          FRAM1.read(FRAM1Address, FRAM1_ROUTE_REC_LEN, b);  // (address, number_of_bytes_to_read, data
          memcpy(&routeElement, b, FRAM1_ROUTE_REC_LEN);
          for (int j = 0; j < FRAM1_RECS_PER_ROUTE; j++) {   // There are up to FRAM1_RECS_PER_ROUTE turnout|block commands per route, look at each one
            // If the first character is a 'T', then add this turnout to the command buffer.
            // The first character could also be 'B' for Block; ignore those here.
            if (routeElement.route[j][0] == 'T') {        // It's a 'T'urnout-type sub-record!
              byte n = atoi(&routeElement.route[j][1]);   // Turnout number is stored as the 2nd and 3rd byte of the array record (followed by 'N' or 'R')
              char d = routeElement.route[j][3];   // I.e. N or R
              if ((d != 'R') && (d != 'N')) {   // Fatal error
                sprintf(lcdString, "Bad route element!");
                sendToLCD(lcdString);
                Serial.println(lcdString);
                endWithFlashingLED(3);
              }
              sprintf(lcdString, "Route turnout %2i %c", n, d);
              sendToLCD(lcdString);
              Serial.println(lcdString);
              // Add this turnout command to the circular turnout command buffer...
              turnoutBufPut(d, n);
            } else if ((routeElement.route[j][0] != 'B') && (routeElement.route[j][0] != ' ')) {   // Not T, B, or blank = error
              sprintf(lcdString, "%.20s", "Bad turnout rec!");
              sendToLCD(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
          }

        } else if (RS485MsgIncoming[3] == '1') {    // 'Park 1' route command, so create a bunch of turnout commands...
          byte routeNum = RS485MsgIncoming[4];
          sprintf(lcdString, "Park 1 route %2i", routeNum);
          sendToLCD(lcdString);
          Serial.println(lcdString);
          // Retrieve route number "routeNum" from Park 1 route Reference table (FRAM1) and create a new record in the
          // turnout command buffer for every turnout in the route...
          unsigned long FRAM1Address = FRAM1_PARK1_START + ((routeNum - 1) * FRAM1_PARK1_REC_LEN); // Rec # vs Route # offset by 1
          byte b[FRAM1_PARK1_REC_LEN];  // create a byte array to hold one Route Reference record
          FRAM1.read(FRAM1Address, FRAM1_PARK1_REC_LEN, b);  // (address, number_of_bytes_to_read, data
          memcpy(&park1Element, b, FRAM1_PARK1_REC_LEN);
          for (int j = 0; j < FRAM1_RECS_PER_PARK1; j++) {   // There are up to FRAM1_RECS_PER_PARK1 turnout|block commands per PARK1 route, look at each one
            // If the first character is a 'T', then add this turnout to the command buffer.
            // The first character could also be 'B' for Block; ignore those here.
            if (park1Element.route[j][0] == 'T') {        // It's a 'T'urnout-type sub-record!
              byte n = atoi(&park1Element.route[j][1]);   // Turnout number is stored as the 2nd and 3rd byte of the array record (followed by 'N' or 'R')
              char d = park1Element.route[j][3];   // I.e. N or R
              if ((d != 'R') && (d != 'N')) {   // Fatal error
                sprintf(lcdString, "%.20s", "Bad Park 1 rec type!");
                sendToLCD(lcdString);
                Serial.println(lcdString);
                endWithFlashingLED(3);
              }
              sprintf(lcdString, "Park 1: %2i %c", n, d);
              sendToLCD(lcdString);
              Serial.println(lcdString);
              // Add this turnout command to the circular turnout command buffer...
              turnoutBufPut(d, n);
            } else if ((park1Element.route[j][0] != 'B') && (park1Element.route[j][0] != ' ')) {   // Not T, B, or blank = error
              sprintf(lcdString, "ERR bad prk1 element");
              sendToLCD(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
          }

        } else if (RS485MsgIncoming[3] == '2') {    // 'Park 2' route command, so create a bunch of turnout commands...
          byte routeNum = RS485MsgIncoming[4];
          sprintf(lcdString, "Park 2 route %2i", routeNum);
          sendToLCD(lcdString);
          Serial.println(lcdString);
          // Retrieve route number "routeNum" from Park 2 route Reference table (FRAM1) and create a new record in the
          // turnout command buffer for every turnout in the route...
          unsigned long FRAM1Address = FRAM1_PARK2_START + ((routeNum - 1) * FRAM1_PARK2_REC_LEN); // Rec # vs Route # offset by 1
          byte b[FRAM1_PARK2_REC_LEN];  // create a byte array to hold one Route Reference record
          FRAM1.read(FRAM1Address, FRAM1_PARK2_REC_LEN, b);  // (address, number_of_bytes_to_read, data
          memcpy(&park2Element, b, FRAM1_PARK2_REC_LEN);
          for (int j = 0; j < FRAM1_RECS_PER_PARK2; j++) {   // There are up to FRAM1_RECS_PER_PARK2 turnout|block commands per PARK2 route, look at each one
            // If the first character is a 'T', then add this turnout to the command buffer.
            // The first character could also be 'B' for Block; ignore those here.
            if (park2Element.route[j][0] == 'T') {        // It's a 'T'urnout-type sub-record!
              byte n = atoi(&park2Element.route[j][1]);   // Turnout number is stored as the 2nd and 3rd byte of the array record (followed by 'N' or 'R')
              char d = park2Element.route[j][3];   // I.e. N or R
              if ((d != 'R') && (d != 'N')) {   // Fatal error
                sprintf(lcdString, "Bad Park 2 rec type!");
                sendToLCD(lcdString);
                Serial.println(lcdString);
                endWithFlashingLED(3);
              }
              sprintf(lcdString, "Park 2: %2i %c", n, d);
              sendToLCD(lcdString);
              Serial.println(lcdString);
              // Add this turnout command to the circular turnout command buffer...
              turnoutBufPut(d, n);
            } else if ((park2Element.route[j][0] != 'B') && (park2Element.route[j][0] != ' ')) {   // Not T, B, or blank = error
              sprintf(lcdString, "%.20s", "ERR bad park2 rec!");
              sendToLCD(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
          }

    
        }  // End of handing this incoming RS485 record, that was a route or park assignment in Auto mode for A-LEG.

      }   // End of "if it's for us"; otherwise msg wasn't for A-SWT, so just ignore (i.e.  no 'else' needed.)

    }  // end of "if RS485 serial available" block

    else {   // We won't even attempt to call turnoutBufProcess() unless the incoming RS485 buffer is empty.

      turnoutBufProcess();  // execute a command from the turnout buffer, if any

    }
  */


//  }  // end of "in any mode, and state is either RUNNING or STOPPING

  paintControlPanel(0);
    
}  // end of main loop()


// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

void initializePinIO() {
  // Rev 7/14/16: Initialize all of the input and output pins
  pinMode(PIN_ROTARY_1, INPUT_PULLUP);
  pinMode(PIN_ROTARY_2, INPUT_PULLUP);
  digitalWrite(PIN_SPEAKER, HIGH);       // Piezo off
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_LED, HIGH);           // Built-in LED off
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_HALT, HIGH);
  pinMode(PIN_HALT, INPUT);              // HALT pin: monitor if gets pulled LOW it means someone tripped HALT.  Or can change to output mode and pull LOW if we want to trip HALT.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);   // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);   // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
  pinMode(PIN_WAV_TRIGGER, INPUT_PULLUP);       // LOW when a track is playing, else HIGH.
  pinMode(PIN_ROTARY_PUSH, INPUT_PULLUP);
  return;
}

/*
void turnoutBufPut(char c, byte i) {
  // Rev: 10/07/16.  Add a new data element to the circular turnout command buffer.
  // Requires GLOBAL turnoutCmdBuf[] command N|R, and turnout number.
  // Requires GLOBAL turnoutBufLen = number of elements in the buffer = maximum capacity
  // 8/5/17: THIS IS BACKWARDS, because data is inserted at the tail and read from the head.
  // CHANGE so we add new data at the head, and retrieve records when needed from the tail.
  
  // If the turnout buffer is full, then it's a fatal error.
  // Insert the new record at the head, and increment the length MOD size of the array
  if (turnoutBufLen < MAX_TURNOUTS_TO_BUF) {
    turnoutCmdBuf[(turnoutBufHead + turnoutBufLen) % MAX_TURNOUTS_TO_BUF].turnoutCmd = c;   // Better be N or R!
    turnoutCmdBuf[(turnoutBufHead + turnoutBufLen) % MAX_TURNOUTS_TO_BUF].turnoutNum = i;
    turnoutBufLen++;
  } else {   // Turnout command buffer overflow.  Fatal!
    sprintf(lcdString, "%.20s", "Turnout buf ovrflw.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(2);   // Turn off all relays/solenoids and request emergency stop
  }
  return;
}

bool turnoutBufGet() {
  // Rev: 10/07/16.  Get a record, if any, from the turnout command buffer.
  // Returns GLOBAL turnoutCmdBuf[] command N|R, and turnout number.
  // Returns 'true' if it puts new values in turnoutCmdBuf[].
  // If the buffer is empty, simply returns 'false'.
  // Needs global variables to get a record, if any
  if (turnoutBufLen > 0) {
    turnoutCmdSingle = turnoutCmdBuf[turnoutBufHead].turnoutCmd;
    turnoutNumSingle = turnoutCmdBuf[turnoutBufHead].turnoutNum;
    // Retrieving a record increases head and decreases length
    turnoutBufHead = (turnoutBufHead + 1) % MAX_TURNOUTS_TO_BUF;
    turnoutBufLen--;
    return true;  // We got a turnout command/number pair!
  } else {
    return false;  // No data retrieved
  }
}

void turnoutBufProcess() {   // Special version for A-LED, not the same as used by A-SWT
  // Rev: 10/26/16.  If a turnout has been thrown, update the turnoutPosition[0..31] array with 'R' or 'N'.
  // AND IF turnoutPosition[] IS UPDATED HERE, then we must also update the turnoutLEDStatus[0..63] array with 0, 1, or 2,
  // which indicates if each LED should be off, on, or blinking.
  // But first, check a timer to be sure that we don't update our turnoutPosition[] array too frequently -- only about
  // as quickly as the actual turnouts are processed by A-SWT.  This is NOT related to the LED flash rate or refresh rate.
  // Note that this function does NOT actually illuminate or darken the green control panel LEDs.
  // We use a 32-char array to maintain the current state of each turnout:
  //   turnoutPosition[0..31] = N, R, or blank.
  // NOTE: array element 0..31 is one less than turnout number 1..32.

  static unsigned long recTimeProcessed = millis();   // To not retrieve turnout buffers too quickly, for effect only.
  
  if ((millis() - recTimeProcessed) > TURNOUT_ACTIVATION_TIME) {   // This is true approx every 1/10 second
    // Performs timer check before checking if a record is available, so we don't light a whole route instantly.

    if  (turnoutBufGet()) {
      // If we get here, we have a new turnout to "throw" *and* it's after a delay.
      recTimeProcessed = millis();  // refresh timer for delay between checking for "throws"

      // Each time we have a new turnout to throw, we need to update TWO arrays:
      // 1: Update turnoutPosition[0..31] for the turnout that was just "thrown."  So we have a list of all current positions.
      // 2: Update turnoutLEDStatus[0..63] to indicate how every green LED *should* now look, with conflicts resolved.

      // For example, we might update turnoutPosition[7] = 'R' to indicate turnout #7 is now in Reverse orientation
      turnoutPosition[turnoutNumSingle - 1] = turnoutCmdSingle;  // 'R' and 'N' are the only valid possibilities

      // Now that we have revised our "turnoutPosition[0..31] = N or R" array, we should fully re-populate our turnoutLEDStatus[] array
      // that indicates which green turnout LEDs should be on, off, or blinking -- taking into account "conflicts."
      // There are 8 special cases, where we have an LED that is shared by two turnouts.  If the LED should bit lit based
      // on one of the pair, but off according to the other turnout, then the shared LED should be blinking.
      // This means that if two facing turnouts, that share an LED, are aligned with each other, then the LED will be solid.
      // If those two turnouts are BOTH aligned away from each other, then the LED will be dark.
      // Finally if one, but not both, turnout is aligned with the other, then the LED will blink.

      // First update the "lit" status of the newly-acquired turnout LED(s) to illuminate Normal or Revers, and turn off it's opposite.
      // This is done via the turnoutLEDStatus[0..63] array, where each element represents an LED, NOT A TURNOUT NUMBER.
      // The offset to the LED number into the turnoutLEDStatus[0..63] array is found in the LEDNormal[0..31] and LEDReverse[0..31] arrays,
      // which are defined as constant byte arrays at the top of the code.
      // I.e. suppose we just read Turnout #7 is now set to Reverse:
      // Look up LEDNormal[6] (turnout #7) =  Centipede shift register pin 6, and LEDReverse[6] (also turnout #7) = Centipede pin 22.
      // So we will want to set LED #6 (7N) off (turnoutLEDStatus[6] = 0), and set LED #22 (7R) on (turnoutLEDStatus[22] = 1).
      // For simplicity, and since this is all that this Arduino does, we'll re-populate the entire turnoutLEDStatus[] array every time
      // we pass through this block -- i.e. every time a turnout changes its orientation.
      // Then we will address any conflicts that should be resolved (facing turnouts that are misaligned.)

      // Here is the "Map" of which turnout position LEDs are connected to which outputs on the Centipede shift registers:
      // 1N = Centipede output 0      1R = Centipede output 16
      // ...                          ...
      // 16N = Centipede output 15    16R = Centipede output 31
      // 17N = Centipede output 32    17R = Centipede output 48
      // ...                          ...
      // 32N = Centipede output 47    32R = Centipede output 63
      // The above "map" is defined in the LEDNormal[0..31] and LEDReverse[0..31] arrays:
      // const byte LEDNormal[TOTAL_TURNOUTS] = 
      //   { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
      // const byte LEDReverse[TOTAL_TURNOUTS] =
      //   {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

      for (byte i = 0; i < TOTAL_TURNOUTS; i++) {
        if (turnoutPosition[i] == 'N') {
          turnoutLEDStatus[LEDNormal[i]] = 1;
          turnoutLEDStatus[LEDReverse[i]] = 0;
        } else if (turnoutPosition[i] == 'R') {
          turnoutLEDStatus[LEDNormal[i]] = 0;
          turnoutLEDStatus[LEDReverse[i]] = 1;
        }
      }

      // Now we need to identify any conflicting turnout orientations, and set the appropriate turnoutLEDStatus[]'s to 2 = blinking.
      // Here is a cross-reference of the possibly conflicting turnouts/orientations and their Centipede shift register pin numbers.
      // We are just going to hard-code this rather than be table-driven, because this isn't a commercial product.
      // NOTE: This code must be modified if used on a new layout, obviously.
      // 1R/3R are pins 16 & 18
      // 2R/5R are pins 17 & 20
      // 6R/7R are pins 21 & 22
      // 2N/3N are pins 1 & 2
      // 4N/5N are pins 3 & 4
      // 12N/13R are pins 11 & 28
      // 17N/18R are pins 32 & 49
      // 27N/28R are pins 42 & 59
      // If one of the above pair is "on" and the other is "off", then both pins (which are connected together) should be set to blink.
      // DON'T FORGET TO SUBTRACT 1 FROM TURNOUT NUMBER WHEN SUBSTITUTING IN TURNOUTPOSITION[]

      if ((turnoutPosition[0] == 'R') && (turnoutPosition[2] == 'N')) {   // Remember turnoutPosition[0] refers to turnout #1
        turnoutLEDStatus[16] = 2;
        turnoutLEDStatus[18] = 2;
      }
      if ((turnoutPosition[0] == 'N') && (turnoutPosition[2] == 'R')) {   // Remember turnoutPosition[0] refers to turnout #1
        turnoutLEDStatus[16] = 2;
        turnoutLEDStatus[18] = 2;
      }

      if ((turnoutPosition[1] == 'R') && (turnoutPosition[4] == 'N')) {
        turnoutLEDStatus[17] = 2;
        turnoutLEDStatus[20] = 2;
      }
      if ((turnoutPosition[1] == 'N') && (turnoutPosition[4] == 'R')) {
        turnoutLEDStatus[17] = 2;
        turnoutLEDStatus[20] = 2;
      }
 
      if ((turnoutPosition[5] == 'R') && (turnoutPosition[6] == 'N')) {
        turnoutLEDStatus[21] = 2;
        turnoutLEDStatus[22] = 2;
      }
      if ((turnoutPosition[5] == 'N') && (turnoutPosition[6] == 'R')) {
        turnoutLEDStatus[21] = 2;
        turnoutLEDStatus[22] = 2;
      }

      if ((turnoutPosition[1] == 'N') && (turnoutPosition[2] == 'R')) {
        turnoutLEDStatus[1] = 2;
        turnoutLEDStatus[1] = 2;
      }
      if ((turnoutPosition[1] == 'R') && (turnoutPosition[2] == 'N')) {
        turnoutLEDStatus[1] = 2;
        turnoutLEDStatus[1] = 2;
      }

      if ((turnoutPosition[3] == 'N') && (turnoutPosition[4] == 'R')) {
        turnoutLEDStatus[3] = 2;
        turnoutLEDStatus[4] = 2;
      }
      if ((turnoutPosition[3] == 'R') && (turnoutPosition[4] == 'N')) {
        turnoutLEDStatus[3] = 2;
        turnoutLEDStatus[4] = 2;
      }

      if ((turnoutPosition[11] == 'N') && (turnoutPosition[12] == 'N')) {
        turnoutLEDStatus[11] = 2;
        turnoutLEDStatus[28] = 2;
      }
      if ((turnoutPosition[11] == 'R') && (turnoutPosition[12] == 'R')) {
        turnoutLEDStatus[11] = 2;
        turnoutLEDStatus[28] = 2;
      }

      if ((turnoutPosition[16] == 'N') && (turnoutPosition[17] == 'N')) {
        turnoutLEDStatus[32] = 2;
        turnoutLEDStatus[49] = 2;
      }
      if ((turnoutPosition[16] == 'R') && (turnoutPosition[17] == 'R')) {
        turnoutLEDStatus[32] = 2;
        turnoutLEDStatus[49] = 2;
      }

      if ((turnoutPosition[26] == 'N') && (turnoutPosition[27] == 'N')) {
        turnoutLEDStatus[42] = 2;
        turnoutLEDStatus[59] = 2;
      }
      if ((turnoutPosition[26] == 'R') && (turnoutPosition[27] == 'R')) {
        turnoutLEDStatus[42] = 2;
        turnoutLEDStatus[59] = 2;
      }
    }
  }
  return;
}

*/

void initializeShiftRegisterPins() {
  // Rev 10/29/16: Set all chips on both Centipede shift register boards to output, high (i.e. turn off all LEDs)
  for (int i = 0; i < 8; i++) {         // For each of 4 chips per board / 2 boards = 8 chips @ 16 bits/chip
    // Set outputs high before setting pin mode to output, to prevent brief low being sent to Centipede shift register outputs
    shiftRegister.portWrite(i, 0b1111111111111111);  // Set all outputs HIGH (pull LOW to turn on an LED)
    shiftRegister.portMode(i, 0b0000000000000000);   // Set all pins on this chip to OUTPUT
  }
  return;
}

void initializeRegInput() {     // Clear data from registrationInput[] to prepare to register trains
  // Rev: 11/10/16
  for (byte i = 0; i < MAX_TRAINS; i++) {    // For every possible "real" train i.e. 1..10
    registrationInput[i].registered = false;
    registrationInput[i].trainNum = 0;
//    strcpy(registrationInput[i].trainName, "        ");
    memset(registrationInput[i].trainName, 0, 9);
    registrationInput[i].lastBlock = 0;
    registrationInput[i].newBlock = 0;
    registrationInput[i].newDirection = ' ';
  }
  return;
}

void initializeBlockReservationTable() {
  // Re-initialize block reservations whenever we begin a new Auto mode.
  for (int i = 0; i < TOTAL_BLOCKS; i++) {
    blockReservation[i].reservedForTrain = TRAIN_ID_NULL;
    blockReservation[i].whichDirection = ' ';
  }
  return;
}

void paintSensorLEDs() {
  // Rev 11/06/16: Based on the array of current sensor status, update every white sensor-status LED on the control panel.
  // NOTE: If currentMode is STOPPED, then we will turn off all white LEDs.
  // If blinking, blink at a rate of toggling every LED_FLASH_TIME milliseconds i.e. every 1/2 second or so.
  // NOTE: We may want to sync the blink timing with paintBlockLEDs() so they flash at the same time.
  // Note: Currently we are not using the "blinking" attrubute for the white occupancy LEDs.

  // At this point, we already have: sensorLEDStatus[0..(TOTAL_SENSORS - 1)] = 0 (off), 1 (on), or 2 (blinking)
  // So now just update the physical LEDs on the control panel, or darken if mode is stopped.
  static unsigned long LEDFlashProcessed = millis();   // Delay between toggling flashing LEDs on control panel i.e. 1/2 second.
  static bool LEDsOn = false;    // Toggle this variable to know when flashing LEDs should be on versus off.  (Flash feature not used yet)

  if ((millis() - LEDFlashProcessed) > LED_FLASH_MS) {   // First decide if flashing LEDs should be on or off at this moment
    LEDFlashProcessed = millis();   // Reset flash timer (about every 1/2 second)
    LEDsOn = !LEDsOn;    // Invert flash toggle
  }

  // Write a ZERO to a bit to turn on the LED, write a ONE to a bit to turn the LED off.  Opposite of our sensorLEDStatus[] array.

  for (byte i = 0; i < TOTAL_SENSORS; i++) {    // For every sensor/"bit" of the Centipede shift register output

    if (stateCurrent == STATE_STOPPED) {   // Darken all white LEDs whenever no mode is running
      shiftRegister.digitalWrite(64 + i, HIGH);   // turn off the LED
    } else  if (modeCurrent == MODE_REGISTER) {  // Darken all white LEDs whenever we are in Register mode, any state
      shiftRegister.digitalWrite(64 + i, HIGH);   // turn off the LED

    // We (no longer as of 9/17) need special handling to resolve sensors #2 and #53, as there is only one LED that they share.
    // No longer applicable: If either one is tripped, then show the LED as lit.  Only if both are clear should the LED be off.
    // Note that as of Sept 2017, we have disconnected sensor 53 from the layout.
    /*
    } else if ((i == 1) || (i == 52)) {     // Special handling if looking at sensor #2 or #53 (note: element 0 is sensor 1, etc.)
      if ((sensorLEDStatus[1] == 0) && (sensorLEDStatus[52] == 0)) {    // Both sensors #2 and #53 are clear
        shiftRegister.digitalWrite(64 + 1, HIGH);        // turn off the LED
        shiftRegister.digitalWrite(64 + 52, HIGH);        // turn off the LED
      } else {                              // At least one of the two is tripped, so illuminate the LED
        shiftRegister.digitalWrite(64 + 1, LOW);        // turn on the LED
        shiftRegister.digitalWrite(64 + 52, LOW);        // turn on the LED
      }

    // For all sensors except #2 and #53...
    */
    // Not STOPPED and not REGISTER means illuminate according to sensor status
    } else if (sensorLEDStatus[i] == 0) {   // LED should be off
      shiftRegister.digitalWrite(64 + i, HIGH);        // turn off the LED
    } else if (sensorLEDStatus[i] == 1) {   // LED should be on
      shiftRegister.digitalWrite(64 + i, LOW);         // turn on the LED
    } else {                                // Must be 2 = blinking.  Note that we are currently not using this feature; for future use if desired.
      if (LEDsOn) {
        shiftRegister.digitalWrite(64 + i, LOW);       // turn on the LED until we have code to handle this
      } else {
        shiftRegister.digitalWrite(64 + i, HIGH);       // turn on the LED until we have code to handle this
      }
    }
  }
  return;
}

void paintBlockLEDs(const byte *tBlockLEDStatus) {
  // Rev 11/06/16: Based on the array of current block status LEDs, update every red/blue LED on the control panel.
  // If blinking, blink at a rate of toggling every LED_FLASH_TIME milliseconds i.e. every 1/2 second or so.
  // NOTE: We may want to sync the blink timing with paintSensorLEDs() so they flash at the same time.
  // tBlockLEDStatus[0..TOTAL_BLOCKS - 1] will be:
  //   LED_DARK = 0;                  // LED off
  //   LED_RED_SOLID = 1;             // RGB block LED lit red solid
  //   LED_RED_BLINKING = 2;          // RGB block LED lit red blinking
  //   LED_BLUE_SOLID = 3;            // RGB block LED lit blue solid
  //   LED_BLUE_BLINKING = 4;         // RGB block LED lit blue blinking

  static unsigned long LEDFlashProcessed = millis();   // Delay between toggling flashing LEDs on control panel i.e. 1/2 second.
  static bool LEDsOn = false;        // Toggle this variable to know when flashing LEDs should be on versus off.

  if ((millis() - LEDFlashProcessed) > LED_FLASH_MS) {   // First decide if flashing LEDs should be on or off at this moment
    LEDFlashProcessed = millis();   // Reset flash timer (about every 1/2 second)
    LEDsOn = !LEDsOn;    // Invert flash toggle
  }

  for (byte block = 0; block < TOTAL_BLOCKS; block++) {     // For block = 0..25, where block number would be 1..26

    if (stateCurrent == STATE_STOPPED) {   // Darken all red and blue LEDs
      shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off
      shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off

//    } else if (modeCurrent == MODE_REGISTER) {
//      shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off
//      shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off

    } else if (tBlockLEDStatus[block] == LED_DARK) {
      shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off
      shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off

    } else if (tBlockLEDStatus[block] == LED_RED_SOLID) {
      shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off
      shiftRegister.digitalWrite(LEDRedBlockPin[block], LOW);  // Red LED on

    } else if (tBlockLEDStatus[block] == LED_BLUE_SOLID) {
      shiftRegister.digitalWrite(LEDBlueBlockPin[block], LOW);  // Blue LED on
      shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off

    } else if (tBlockLEDStatus[block] == LED_RED_BLINKING) {
      shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off

      if (LEDsOn) {   // Flashing LEDs "on" at this moment
        shiftRegister.digitalWrite(LEDRedBlockPin[block], LOW);  // Red LED on

      } else {  // Flashing LEDs "off" at this moment
        shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off
      }

    } else if (tBlockLEDStatus[block] == LED_BLUE_BLINKING) {
      shiftRegister.digitalWrite(LEDRedBlockPin[block], HIGH);  // Red LED off

      if (LEDsOn) {   // Flashing LEDs "on" at this moment
        shiftRegister.digitalWrite(LEDBlueBlockPin[block], LOW);  // Blue LED on

      } else {  // Flashing LEDs "off" at this moment
        shiftRegister.digitalWrite(LEDBlueBlockPin[block], HIGH);  // Blue LED off
      }

    }
  }
  return;
}

void paintControlPanel(byte tItemNum) {
  // Rev 02/22/17: Added logic for different modes.
  // Update all white occupancy sensor LEDs and red/blue block occupancy LEDs.
  // Operates differently depending on mode and state.  Uses global variables currentMode and currentState.
  // Uses many global variables and arrays including sensorLEDStatus[], trainProgress[], and blockReservation[].
  // Passed parm tItemNum can either be n/a, a block number, or a train number, depending on mode.

  // sensorLEDStatus[0..TOTAL_SENSORS - 1] will already be set to 0 or 1 depending on if a sensor is clear or tripped.
  // The paintSensorLEDs() function will darken all white LEDs if the state is Stopped.
  paintSensorLEDs();   // This will update the white LEDs.  If currentState is STOPPED, they will all be darkened.

  // blockLEDStatus[0..TOTAL_BLOCKS - 1] will be set below, based on modes etc., to:
  //   LED_DARK = 0;                  // LED off
  //   LED_RED_SOLID = 1;             // RGB block LED lit red solid
  //   LED_RED_BLINKING = 2;          // RGB block LED lit red blinking
  //   LED_BLUE_SOLID = 3;            // RGB block LED lit blue solid
  //   LED_BLUE_BLINKING = 4;         // RGB block LED lit blue blinking
  // Set up an array to store the color/status of every red/blue LED.
  // We will pass a pointer to this array, to the paintBlockLEDs() function.
  byte blockLEDStatus[TOTAL_BLOCKS];
  for (byte b = 0; b < TOTAL_BLOCKS; b++) {    // Init to all off, and we will populate according to mode, then paint them.
    blockLEDStatus[b] = LED_DARK;
  }
    
  if (stateCurrent != STATE_STOPPED) {   // Don't bother with logic here if the state is stopped, since all red/blue LEDs will be painted dark automatically in that case.

    // In MANUAL/POV, uses sensorLEDStatus[] to know how to paint red/blue LEDs.  tItemNum parameter is unused (set to zero.)  We simply examine sensorLEDStatus[]
    //   and sensorBlock[], and illuminate a block LED (solid blue) if *either* occupancy sensor for that block is tripped, else LED should be dark.
    //   This is sort of unreliable, in the case that a train is in a block *between* sensors, and both are clear.  Maybe we should not illuminate red/blue LEDs
    //   when operating in Manual or POV mode...???
    if ((modeCurrent == MODE_MANUAL) || (modeCurrent == MODE_POV)) {   // Paint just one blue LED per occupied block.
      for (byte sensor = 0; sensor < TOTAL_SENSORS; sensor++) {
        if (sensorLEDStatus[sensor] > 0) {    // if it's occupied (lit solid or blinking)
          blockLEDStatus[sensorBlock[sensor].whichBlock - 1] = LED_BLUE_SOLID;
        }
      }
    }

    // In REGISTER mode, tItemNum will be a BLOCK NUMBER to be lit in SOLID RED, or 99 (TRAIN_STATIC) to turn off all LEDs during init.
    //   tItemNum will normally indicate which block the system is prompting the operator to identify a train in.  All other sensor and block
    //   LEDs will remain dark.
    //   FUTURE ENHANCEMENT: If desired, we can illuminate all not-yet-identified occupied blocks in SOLID RED, and all previously-identified occupied blocks in
    //   SOLID BLUE.  I.e. the display will change from all red to all blue as block occupancy is specified.  Don't bother with this yet, thought; keep it simple.
    else if (modeCurrent == MODE_REGISTER) {
//      if (tItemNum != TRAIN_STATIC) {
        blockLEDStatus[tItemNum - 1] = LED_RED_SOLID;  // Remember blockLEDStatus[] is a zero-offset array, for blocks starting with 1.
//      }
    }

    // In AUTO/PARK, full treatment of red/blue LEDs: full routes, occupied blocks blinking, color according to TRAIN tItemNum (selected train)
    //   Uses global trainProgress[] structure to know how to paint red/blue LEDs for each registered train, and blockReservation[] to know which other
    //   red/blue LEDs should be lit (to indicate Static trains.)
    //   When in AUTO/PARK mode, A-OCC permits the operator to use the rotary encoder to scroll among all *registered* trains, selecting one at a time.
    //   The selected train will be passed here via the tItemNum (TRAIN NUMBER) parameter [1..MAX_TRAINS].  If zero, do not use that feature.
    //   Using the Train Progress table, illuminate the red/blue LEDs as follows:
    //     Using the Block Reservation table, every block reserved for Train #TRAIN_STATIC (static) shall be lit in solid blue.
    //     Using the Train Progress table, for every active train, for every record in that train's trainProgress[] table:
    //       If either sensor (entry or exit) for that block is tripped, *or* if this is the "tail" of the train:
    //         For train tItemNum, illuminate that block in flashing red.
    //         For all other trains (*except* the one passed as tItemNum), illuminate that block in flashing blue.
    //       Otherwise (if neither sensor (entry or exit) for that block is tripped, *and* this is *not* the "tail" of the train):
    //         For train tItemNum, illuminate that block in solid red.
    //         For all other trains (*except* the one passed as tItemNum), illuminate that block in solid blue.
    //   The above algorithm has some logic to guard against the case where a train is between two cleared sensors in a block, that if it's the tail of the Train
    //     Progress entry for that train, we shall assume that the train is there, and simply between the sensors.  Which should be foolproof, because we know that
    //     if the tail exit sensor had *cleared* (rather than never been tripped), we would have removed that record from the table.  Thus we do not have the
    //     problem we would have in Manual mode if a train was between two sensors -- there is no way to know it's there.
    else if ((modeCurrent == MODE_AUTO) || (modeCurrent == MODE_PARK)) {





    }
  }  // end of "if state is not STOPPED

  paintBlockLEDs(blockLEDStatus);  // Paint all of the red/blue LEDs.  If currentState is STOPPED, they will all be darkened.
  return;

}

// **************************************************************
// ********************** RS485 FUNCTIONS ***********************
// **************************************************************

bool RS485fromMAStoALL_ModeMessage() {
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_ALL) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'M') {  // It's a Mode update from A-MAS!
        return true;
      }
    }
  }
  return false;
}

void RS485UpdateModeState(byte * tModeOld, byte * tModeCurrent, bool * tModeChanged, byte * tStateOld, byte * tStateCurrent, bool * tStateChanged) {
  if (* tModeCurrent != RS485MsgIncoming[4]) {   // We have a new MODE
    * tModeOld = * tModeCurrent;
    * tModeCurrent = RS485MsgIncoming[4];
    * tModeChanged = true;
  }
  if (* tStateCurrent != RS485MsgIncoming[5]) {  // We have a new STATE
    * tStateOld = * tStateCurrent;
    * tStateCurrent = RS485MsgIncoming[5];
    * tStateChanged = true;
  }
}

bool RS485fromSNStoMAS_SensorMessage() {
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_MAS) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_SNS) {
      if (RS485MsgIncoming[3] == 'S') {  // It's a sensor-status update from A-SNS
        return true;
      }
    }
  }
  return false;
}

bool RS485fromMAStoOCC_Question() {
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_OCC) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'Q') {  // It's a Question/Query request from A-MAS (operator prompt)
        return true;
      }
    }
  }
  return false;
}

bool RS485fromMAStoOCC_Registration() {
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_OCC) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'R') {  // It's a Registration request from A-MAS (operator prompt)
        return true;
      }
    }
  }
  return false;
}

// ****************************************************************************
// ****************** TRAIN PROGRESS FUNCTIONS Rev: 09/28/17 ******************
// ****************************************************************************

void trainProgressInit(const byte tTrainNum) {
  // Rev: 09/05/17
  // When we first start registration mode, we need to initialize all possible trains to empty; no blocks reserved/occupied.
  // This routine will initialize a SINGLE TRAIN, so we must call it once for each possible train.
  // Input: train number to initialize (1..MAX_TRAINS)
  trainProgress[tTrainNum - 1].head = 0;
  trainProgress[tTrainNum - 1].tail = 0;
  trainProgress[tTrainNum - 1].count = 0;
  for (byte i = 0; i < MAX_BLOCKS_PER_TRAIN; i++) {
    trainProgress[tTrainNum - 1].blockNum[i] = 0;
    trainProgress[tTrainNum - 1].entrySensor[i] = 0;
    trainProgress[tTrainNum - 1].exitSensor[i] = 0;
  }
  return;
}

bool trainProgressIsEmpty(const byte tTrainNum) {
  // Rev: 09/06/17
  // Returns true if tTrainNum's train progress table has zero valid records; else false.
  if ((tTrainNum > 0) && (tTrainNum <= MAX_TRAINS)) {  // Looking at actual train numbers 1..MAX_TRAINS
    return (trainProgress[tTrainNum - 1].count == 0);
  } else {
    Serial.println("FATAL ERROR!  Train number out of range in trainProgressIsEmpty."); // *********************** CHANGE TO STANDARD ALL-STOP FATAL ERROR IN REGULAR CODE
    while (true) {}
  }
}

bool trainProgressIsFull(const byte tTrainNum) {
  // Rev: 09/06/17
  // Returns true if tTrainNum's train progress table has the maximum possible records; else false.
  if ((tTrainNum > 0) && (tTrainNum <= MAX_TRAINS)) {  // Looking at actual train numbers 1..MAX_TRAINS
    return (trainProgress[tTrainNum - 1].count == MAX_BLOCKS_PER_TRAIN);
  } else {
    Serial.println("FATAL ERROR!  Train number out of range in trainProgressIsFull."); // *********************** CHANGE TO STANDARD ALL-STOP FATAL ERROR IN REGULAR CODE
    while (true) {}
  }
}

void trainProgressEnqueue(const byte tTrainNum, const byte tBlockNum, const byte tEntrySensor, const byte tExitSensor) {
  // Rev: 09/29/17.
  // Adds a single record (block number, entry sensor, exit sensor) for a given train number to the circular buffer.
  // Inputs are: train number, block number, entry sensor number, and exit sensor number.
  // Fatal error and halt if buffer already full.
  // If not full, insert data at head, then increment head (modulo size), and increments count.
  if (!trainProgressIsFull(tTrainNum)) {    // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    byte tHead = trainProgress[tTrainNum - 1].head;
    // byte tCount = trainProgress[tTrainNum - 1].count;   I GUESS I DON'T NEED THIS TEMP VARIABLE...???
    trainProgress[tTrainNum - 1].blockNum[tHead] = tBlockNum;
    trainProgress[tTrainNum - 1].entrySensor[tHead] = tEntrySensor;
    trainProgress[tTrainNum - 1].exitSensor[tHead] = tExitSensor;
    trainProgress[tTrainNum - 1].head = (tHead + 1) % MAX_BLOCKS_PER_TRAIN;
    trainProgress[tTrainNum - 1].count++;
  } else {  // Train Progress table is full; how could this happen?
    sprintf(lcdString, "%.20s", "Train progress full!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  return;
}

bool trainProgressDequeue(const byte tTrainNum, byte * tBlockNum, byte * tEntrySensor, byte * tExitSensor) {
  // Rev: 10/01/17.
  // Retrieves and clears a single record for a given train number, from the circular buffer.
  // Because this function returns values via the passed parameters, CALLS MUST SEND THE ADDRESS OF THE RETURN VARIABLES, i.e.:
  //   status = trainProgressDequeue(trainNum, &blockNum, &entrySensor, &exitSensor);
  // And because we are passing addresses, our code inside this function must use the "*" dereference operator.
  // Input is: train number.
  // Returns true if successful; false if buffer already empty.
  // If not empty, returns block number, entry sensor, and exit sensor AS PASSED PARAMETERS.
  // Data will be at tail, then tail will be incremented (modulo size), and count will be decremented.
  if (!trainProgressIsEmpty(tTrainNum)) {    // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    byte tTail = trainProgress[tTrainNum - 1].tail;
    *tBlockNum = trainProgress[tTrainNum - 1].blockNum[tTail];
    *tEntrySensor = trainProgress[tTrainNum - 1].entrySensor[tTail];
    *tExitSensor = trainProgress[tTrainNum - 1].exitSensor[tTail];
    trainProgress[tTrainNum - 1].tail = (tTail + 1) % MAX_BLOCKS_PER_TRAIN;
    trainProgress[tTrainNum - 1].count--;
    return true;
  } else {    // Train Progress table is empty
    return false;
  }
}

bool trainProgressFindSensor(const byte tTrainNum, const byte tSensorNum, byte * tBlockNum, byte * tEntrySensor, byte * tExitSensor, bool * tLocHead, bool * tLocTail, bool * tLocPenultimate) {
  // Rev: 09/30/17.
  // Search every active block of a given train, until given sensor is found or not part of this train's route.
  // Must be called for every train, until desired sensor is found as either and entry or exit sensor.
  // Because this function returns values via the passed parameters, CALLS MUST SEND THE ADDRESS OF THE RETURN VARIABLES, i.e.:
  //   status = trainProgressFindSensor(trainNum, sensorNum, &blockNum, &entrySensor, &exitSensor, &locHead, &locTail, &locPenultimate);
  // And because we are passing addresses, our code inside this function must use the "*" dereference operator.
  // Inputs are: train number, sensor number to find.
  // Returns true if the sensor was found as part of this train's route; false if the sensor was not part of the train's route.
  // If not found, then the values for all other returned parameters will be undefined/garbage.  Can't think of a reason to initialize them i.e. to
  // zero and false, but we easily could.
  // If found, returns the block number, entry sensor, and exit sensor AS PASSED PARAMETERS.
  // If found, also returns if the block is the head, tail, or penultimate block in the train's route.  We use three separate bool variables for this,
  // rather than an enumeration, because the sensor could be up to two of these (but not all three,) such as both the head and the tail if the route
  // only had one block, or both the tail and the penultimate sensor if the route only had two blocks.
  // IMPORTANT RE: HEAD!!!  The "head" that we track internally is actually one in front of the "highest" active element (unless zero elements.)
  // I.e. in our circular buffers, head points to the location where the next new element will be added.
  // When we return the bool tLocHead, we mean is the sensor found in the highest active element, one less than the head pointer (unless zero elements.)
  // Let's set all of the return values to zero and false just for good measure...
  * tBlockNum = 0;
  * tEntrySensor = 0;
  * tExitSensor = 0;
  * tLocHead = false;
  * tLocTail = false;
  * tLocPenultimate = false;
  // The sensor certainly won't be found if this train's progress table is empty, but perhaps we want to check that before even calling this routine,
  // in which case the following block can be removed as redundant.
  if (!trainProgressIsEmpty(tTrainNum)) {     // We do *not* want to pass tTrainNum - 1; this is our "actual" train number
    // Start at tail and increment using modulo addition, traversing/searching each active block
    // tElement starts pointing at tail and will be incremented (modulo size) until it points at head
    byte tElement = trainProgress[tTrainNum - 1].tail;
    // i is the number of iterations; the number of elements in this train's (non-empty) table 1..MAX_BLOCKS_PER_TRAIN
    for (byte i = 0; i < trainProgress[tTrainNum - 1].count; i++) {
      if ((trainProgress[tTrainNum - 1].entrySensor[tElement] == tSensorNum) || (trainProgress[tTrainNum - 1].exitSensor[tElement] == tSensorNum)) {
        // Found it at this block's entry or exit sensor!
        * tBlockNum = trainProgress[tTrainNum - 1].blockNum[tElement];
        * tEntrySensor = trainProgress[tTrainNum - 1].entrySensor[tElement];
        * tExitSensor = trainProgress[tTrainNum - 1].exitSensor[tElement];
        if (tElement == trainProgress[tTrainNum - 1].tail) {
          *tLocTail = true;
        }
        if (((tElement + 1) % MAX_BLOCKS_PER_TRAIN) == trainProgress[tTrainNum - 1].head) {  // Head in terms of highest active element, one less than head pointer
          * tLocHead = true;
        }
        if (((tElement + 2) % MAX_BLOCKS_PER_TRAIN) == trainProgress[tTrainNum - 1].head) {  // Head in terms of highest active element, one less than head pointer
          * tLocPenultimate = true;
        }
        return true;   // We found the sensor in the current block!
      }
      tElement = (tElement + 1) % MAX_BLOCKS_PER_TRAIN;  // We didn't find the sensor here, so point to the next element (if not past head) and try again
    }
    return false;
  } else {  // Train Progress table is empty
    return false;
  }
}

void trainProgressDisplay(const byte tTrainNum) {
  // Rev: 09/06/17.
  // Display (to the serial monitor) the entire Train Progress table for a given train, starting at tail and working to head.
  // This is just used for debugging and can be removed from final code.
  // Iterates from tail, and prints out count number of elements, wraps around if necessary.
  if (!trainProgressIsEmpty(tTrainNum)) {   // In this case, actual train number tTrainNum, not tTrainNum - 1
    Serial.print("Train number: "); Serial.println(tTrainNum);
    Serial.print("Is Empty? "); Serial.println(trainProgressIsEmpty(tTrainNum));
    Serial.print("Is Full?  "); Serial.println(trainProgressIsFull(tTrainNum));
    Serial.print("Count:    "); Serial.println(trainProgress[tTrainNum - 1].count);
    Serial.print("Head:     "); Serial.println(trainProgress[tTrainNum - 1].head);
    Serial.print("Tail:     "); Serial.println(trainProgress[tTrainNum - 1].tail);
    Serial.print("Data:     "); 
    // Start at tail and increment using modulo addition, traversing/searching each active block
    // tElement starts pointing at tail and will be incremented (modulo size) until it points at head
    byte tElement = trainProgress[tTrainNum - 1].tail;
    // i is the number of iterations; the number of elements in this train's (non-empty) table 1..MAX_BLOCKS_PER_TRAIN
    for (byte i = 0; i < trainProgress[tTrainNum - 1].count; i++) {   
      Serial.print(trainProgress[tTrainNum - 1].blockNum[tElement]); Serial.print(", ");
      Serial.print(trainProgress[tTrainNum - 1].entrySensor[tElement]); Serial.print(", ");
      Serial.print(trainProgress[tTrainNum - 1].exitSensor[tElement]); Serial.print(";   ");
      tElement = (tElement + 1) % MAX_BLOCKS_PER_TRAIN;
    }
    Serial.print("End of train "); Serial.println(tTrainNum);
  }
  else {
    Serial.print("Train number "); Serial.print(tTrainNum); Serial.println("'s Train Progress table is empty.");
  }
}

// **************************************************************
// ******************** 8-CHAR A/N FUNCTIONS ********************
// **************************************************************

void sendToAlpha(const char nextLine[])  {
  // Rev: 02/04/17, based on sendToAlpha Rev 10/14/16 - TimMe and RDP
  // The char array that holds the data is 9 bytes long, to allow for an 8-byte text msg (max.) plus null char.
  // Sample calls:
  //   char alphaString[9];  // Global array to hold strings sent to Adafruit 8-char a/n display.
  //   sendToAlpha("WP  83");   Note: more than 8 characters will crash!
  //   sprintf(alphaString, "WP  83"); 
  //   sendToAlpha(alphaString);   i.e. "WP  83"
  Serial.println(strlen(nextLine));
  if ((nextLine == (char *)NULL) || (strlen(nextLine) > 8)) endWithFlashingLED(2);
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
  return;
}

// ************************************************************************
// ******************** ROTARY ENCODER FUNCTIONS **************************
// ************************************************************************

void ISR_rotary_push() {
  // Rev: 10/26/16.  Set rotaryPushed = true, after waiting for bounce to settle.
  // We can eliminate delay if we implement hardware debounce, but it only takes 5 milliseconds, and we are only
  // delayed when the operator presses the button, so this is nothing.  Just keep software debounce.
  delayMicroseconds(5000);   // 1000 microseconds is not quite enough to totally eliminate bounce.
  if (digitalRead(PIN_ROTARY_PUSH) == LOW) {
    rotaryPushed = true;
  }
  return;
}  

void ISR_rotary_turn() {
  // Rev: 10/26/16. Called when rotary encoder turned either direction, returns rotaryTurned and rotaryNewState globals.
  static unsigned char state = 0;
  unsigned char pinState = (digitalRead(PIN_ROTARY_2) << 1) | digitalRead(PIN_ROTARY_1);
  state = ttable[state & 0xf][pinState];
  unsigned char stateReturn = (state & 0x30);
  if ((stateReturn == DIR_CCW) || (stateReturn == DIR_CW)) {
    rotaryTurned = true;
    rotaryNewState = stateReturn;
  }
  return;
}

void ISR_rotary_enable() {
  // Rev: 02/04/17.  Call this to ENABLE interrupts by the rotary encoder.
  attachInterrupt(digitalPinToInterrupt(PIN_ROTARY_1), ISR_rotary_turn, CHANGE);  // Interrupt 0 is Mega pin 2
  attachInterrupt(digitalPinToInterrupt(PIN_ROTARY_2), ISR_rotary_turn, CHANGE);  // Interrupt 1 is Mega pin 3
  attachInterrupt(digitalPinToInterrupt(PIN_ROTARY_PUSH), ISR_rotary_push, FALLING); // Interrupt 4 is Mega pin 19
  return;
}

void ISR_rotary_disable() {
  // Rev: 02/04/17.  Call this to DISABLE interrupts by the rotary encoder.
  detachInterrupt(digitalPinToInterrupt(PIN_ROTARY_1));  // Interrupt 0 is Mega pin 2
  detachInterrupt(digitalPinToInterrupt(PIN_ROTARY_2));  // Interrupt 1 is Mega pin 3
  detachInterrupt(digitalPinToInterrupt(PIN_ROTARY_PUSH)); // Interrupt 4 is Mega pin 19
  return;
}

// *********************************************************************************
// ***************** ROTARY ENCODER ALPHANUMERIC PROMPT FUNCTIONS ******************
// *********************************************************************************

void initRotaryPrompts() {
  // Clear out the entire list of possible prompts, even if we may only use some of them
  for (byte t = 0; t < MAX_TRAINS + 2; t++) {   // (Zero offset) For every possible prompt (max would be max trains + 2)
    rotaryPrompt[t].promptNum = 0;
    strncpy(rotaryPrompt[t].promptText, " ", 8);
    char nullChar[1] = {'\0'};
    memcpy(rotaryPrompt[t].promptText + 8, nullChar, 1);
    rotaryPrompt[t].lastBlock = 0;
  }
  return;
}

byte getRotaryResponse(byte tStart) {
  // Use the global structure rotaryPrompt[] of 8-char a/n prompts, and wait until the operator selects one.
  // Passed value "tStart" indicates index into rotaryPrompt[] (0..n) to use as initial prompt; i.e. default when selecting trains.
  // Global variable numRotaryPrompts will tell us how many elements in the prompts array to use (elements 0..numRotaryPrompts - 1)
  // We need MAX_TRAINS+2 because we might send a list of every train, and also have options for STATIC and DONE.
  // Returns the array offset into rotaryPrompt[] (0..n) of the selected option.

  // First of all, let's turn on interrupts so we can see changes to the rotary encoder (turns plus press)...
  ISR_rotary_enable();
  byte responseNum = tStart;  // Default first prompt index into rotaryPrompt[] array
  if (tStart > numRotaryPrompts - 1) {  // This should never happen
        sprintf(lcdString, "%.20s", "tStart too large!");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        endWithFlashingLED(1);
  }
  char s[9];  // Holds a complete prompt string to be sent to the A/N display
  rotaryPushed = false;  // Reset this value
  rotaryTurned = false;
  // Display the first prompt
  memcpy(s, rotaryPrompt[responseNum].promptText, 9);
  sendToAlpha(s);

  while (rotaryPushed == false) {   // Keep updating the prompt when rotary turned, until it is pressed
    if (rotaryTurned == true) {      // Set to true by an interrupt when turned either direction
      if (rotaryNewState == DIR_CCW) {  // Rotary was turned counter-clockwise
        if (responseNum > 0) {
          responseNum = responseNum - 1;     // Just decriment it if we won't make it less than zero
        } else {
          responseNum = numRotaryPrompts - 1;   // It's zero, so set to highest prompt (modulo subtraction)
        }
      } else if (rotaryNewState == DIR_CW) {   // Rotary was turned clockwise, so display the next higher prompt (modulo number-of-prompts)
        responseNum = ((responseNum + 1) % numRotaryPrompts);   // Add 1, modulo total number of prompts
      } else {    // Error - this should never happen
        sprintf(lcdString, "%.20s", "Rotary DIR bad!");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        endWithFlashingLED(1);
      }
      // Operator turned the dial and we've updated responseNum, so display it now
      memcpy(s, rotaryPrompt[responseNum].promptText, 8);
      sendToAlpha(s);
      rotaryTurned = false;  // Reset this
    }
  }  // When we fall out of this loop, the operator has pressed the rotary, and thus selected prompt responseNum
  // Since we're done using the rotary encoder, disable interrupts so we won't be bothered if operator plays with it...
  ISR_rotary_disable();
  return responseNum;    // Will be in the range of 0..numRotaryPrompts - 1
}

byte promptOperatorForTrain(byte tBlock) {
  // Parameter tBlock indicates the "real" block number that we are prompting about (1..TOTAL_BLOCKS, not zero offset)
  // Returns the train number, or TRAIN_STATIC (99) for Static, or TRAIN_DONE (100) for Done
  // Uses data in registrationInput[] but does not update; that's done by the calling routine.  Record 0 corresponds to train 1.
  // We start with totalTrainsReceived records (0..tTR - 1) in registrationInput, including:
  // registrationInput[n].registered = true or false;
  // registrationInput[n].trainNum = always set to train number (n + 1);
  // registrationInput[n].trainName = 8-char description of train
  // registrationInput[n].lastBlock = best guess as to which block this train is in
  // registrationInput[n].newBlock = starts with zero, updated to actual block if train is registered.
  // registrationInput[n].newDirection = starts with blank, updated to actual direction if train is registered

  // Limit our selections to registrationInput[] records where .registered = false (set true by calling routine if/when registered)
  // In the unlikely event that all trains get registered, then return TRAIN_DONE, as if operator has selected Done.
  // When cycling through the various train names as prompts, start with the train (if any) with a default (last-known) block of tBlock.
  // Paint the control panel with ONLY the one block we are asking about lit, all other LEDs off.
  paintControlPanel(tBlock);
  initRotaryPrompts();  // Reset all prompts to blank; maybe not necessary since we will populate each time
  // numRotaryPrompts should be set to the number of prompts to use in rotaryPrompt[].
  numRotaryPrompts = 0;  // This will be set to the "real" number of prompts i.e. 1..n (not an offset into a zero-indexed array)
  // Populate rotaryPrompt[] with every train description that has not been registered yet, plus STATIC and DONE.
  for (byte b = 0; b < MAX_TRAINS; b++) {  // For every entry in the registrationInput[] table (MAX_TRAINS)
    if (registrationInput[b].registered == false) {  // Not yet registered, so add it to rotaryPrompt[]
      memcpy(rotaryPrompt[numRotaryPrompts].promptText, registrationInput[b].trainName, 8);
      rotaryPrompt[numRotaryPrompts].promptNum = registrationInput[b].trainNum;     // NEW CODE 2/21/17
      rotaryPrompt[numRotaryPrompts].lastBlock = registrationInput[b].lastBlock;   // Last-known block occupied by this train
      numRotaryPrompts++;  // Reflects the actual number of records, not the record number (zero offset)
    }
  }
  // If we didn't find any trains that have not yet been registered, then we're done
  if (numRotaryPrompts == 0) {
    return TRAIN_DONE_ID;
  }
  // If we get here, then we have at least one train that has not yet been registered, for the operator to select from.
  // Now add STATIC and DONE to rotaryPrompt[]
  memcpy(rotaryPrompt[numRotaryPrompts].promptText, "STATIC  ", 8);
  rotaryPrompt[numRotaryPrompts].promptNum = TRAIN_ID_STATIC;
  rotaryPrompt[numRotaryPrompts].lastBlock = 0;
  numRotaryPrompts++;
  memcpy(rotaryPrompt[numRotaryPrompts].promptText, "DONE    ", 8);
  rotaryPrompt[numRotaryPrompts].promptNum = TRAIN_DONE_ID;
  rotaryPrompt[numRotaryPrompts].lastBlock = 0;
  numRotaryPrompts++;

  // Give getRotaryResponse a default starting element in rotaryPrompt[]; not necessarily the same as the train number, as trains get registered i.e. Train 7
  // could be in element 4.
  // See if any not-yet-registered train has a last-known block of the block we are prompting for, and use that train's rotaryPrompt[] index.
  // Otherwise just pass getRotaryResponse() a zero.
  byte bestGuess = 0;
  for (byte b = 0; b < numRotaryPrompts - 2; b++) {   // Exclude STATIC and DONE.  So if just one train, numRotaryPrompts would be 3, and loop would run once.
    if (rotaryPrompt[b].lastBlock == tBlock) {   // Hey! The train stored in rotaryPrompt[b] has a last-known block of the block we are trying to register a train for!
      bestGuess = b;  // bestGuess is an index number of the rotaryPrompt array i.e. 0..n-1
    }
  }
  // Now ask the operator to select a train for this block
  // Pass byte "bestGuess" as the index 0..n into the rotaryPrompt[] array to use as default train for the block being prompted for.
  byte tResponse = getRotaryResponse(bestGuess);  // tResponse will be an index into rotaryPrompt[] starting with zero for first element.
  // Now tResponse is the array index into rotaryPrompt[] (0..n-1) that the operator selected; either a train or STATIC or DONE.
  // Recall that rotaryPrompt[].promptNum is, in this case, the train number 1..MAX_TRAINS *or* STATIC *or* DONE
  sprintf(lcdString, "Train %2d %8s", rotaryPrompt[tResponse].promptNum, rotaryPrompt[tResponse].promptText);
  sendToLCD(lcdString);  

  return rotaryPrompt[tResponse].promptNum;  // This will be either the "real" train number 1..MAX_TRAINS, or TRAIN_STATIC (99), or TRAIN_DONE (100)
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

void initializeFRAM1AndGetControlBlock() {
  // Rev 09/26/17: Initialize FRAM chip(s), get chip data and control block data including confirm
  // chip rev number matches code rev number.

  if (FRAM1.begin())  // Any non-zero result is an error
  {
    // Here are the possible responses from FRAM1.begin():
    // ferroOK = 0
    // ferroBadStartAddress = 1
    // ferroBadNumberOfBytes = 2
    // ferroBadFinishAddress = 3
    // ferroArrayElementTooBig = 4
    // ferroBadArrayIndex = 5
    // ferroBadArrayStartAddress = 6
    // ferroBadResponse = 7
    // ferroPartNumberMismatch = 8
    // ferroUnknownError = 99
    sprintf(lcdString, "%.20s", "FRAM1 bad response.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(1);
  }

  if (FRAM1.getControlBlockSize() != 128) {
    sprintf(lcdString, "%.20s", "FRAM1 bad blk size.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(1);
  }

  FRAM1BufSize = FRAM1.getMaxBufferSize();  // Should return 128 w/ modified library value
  FRAM1Bottom = FRAM1.getBottomAddress();      // Should return 128 also
  FRAM1Top = FRAM1.getTopAddress();            // Should return 8191

  FRAM1.readControlBlock(FRAM1ControlBuf);
  for (byte i = 0; i < 3; i++) {
    FRAM1GotVersion[i] = FRAM1ControlBuf[i];
    if (FRAM1GotVersion[i] != FRAM1VERSION[i]) {
      sprintf(lcdString, "%.20s", "FRAM1 bad version.");
      sendToLCD(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
  }

  // A-MAS will also retrieve last-known-turnout and last-known-train positions from control block, but nobody else needs this.
  return;
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

