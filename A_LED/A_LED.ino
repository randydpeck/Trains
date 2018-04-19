// A_LED Rev: 12/03/17.
char APPVERSION[21] = "A-LED Rev. 12/03/17";

// Include the following #define if we want to run the system with just the lower-level track.
#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

// A_LED controls the green control panel turnout-indication LEDs via Centipede shift registers.
// This is an "INPUT-ONLY" module that does not provide data to any other Arduino.
// If two facing turnouts that are represented by a single LED are not aligned together, the green LED blinks.
// Listens for RS485 incoming commands (directed at A-SWT) to know how turnouts are set.
// However, paints all green LEDs "off" if state is not STATE_RUNNING or STATE_STOPPING, regardless of mode.

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

// IMPORTANT: LARGE AMOUNTS OF THIS CODE ARE IDENTIAL IN A-SWT, SO ALWAYS UPDATE A-SWT WHEN WE MAKE CHANGES TO THIS CODE.
// 09/16/17: Changed variable suffixes from Length to Len, No/Number to Num, Record to Rec, etc.
// 08/29/17: Cleaning up code, updated RS485 message protocol comments.
// 02/26/17: Added mode and state changed = false at top of get RS485 message main loop
// 02/24/17: Added code to monitor MODE and STATUS, and to darken all green LEDs when status = stopped.
// 11/14/16: Substituted const byte LED_DARK, LEG_GREEN_SOLKD, and LED_GREEN_BLINKING for 0, 1, and 2 in code.
// 11/02/16: initializeShiftRegisterPins() and delay in sendToLCD()
// 11/02/16: Added LCD messages when messages received and turnouts thrown, to match A-SWT.
// 10/19/16: extended length of delay when checking if Halt pressed.
// 10/19/16: Modified to "blink" LEDs that represent two turnouts that connect and are set differently.

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-LED  (same as A-SWT.)  Byte numbers represent offsets, so they start at zero. ***
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

// A-MAS to A-SWT:  Command to set an individual turnout
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 6
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  [N|R] Normal|Reverse
//    4    Parameter Byte  Turnout number 1..32
//    5	   Checksum	 Byte  0..255

// A-MAS to A-SWT:  Command to set a Route (regular, or Park 1 or Park 2)
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 6
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  [T|1|2] rouTe|park 1|park 2
//    4    Parameter Byte  Route number from one of the three route tables, 1..70, 1..19, or 1..4
//    5	   Checksum	 Byte  0..255

// A-MAS to A-SWT:  Command to set all turnouts to Last-known position
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 9
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  'L' = Last-known position
//    4..7 Parameter Byte  32 bits: 0=Normal, 1=Reverse
//    8	   Checksum  Byte  0..255

// **************************************************************************************************************************

#include <Train_Consts_Global.h>
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?
byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
byte RS485MsgOutgoing[RS485_MAX_LEN];

char lcdString[LCD_WIDTH + 1];                   // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// We will start in MODE_UNDEFINED, STATE_STOPPED.  We must receive a message from A-MAS to tell us otherwise.
byte modeCurrent     = MODE_UNDEFINED;
byte modeOld         = modeCurrent;
bool modeChanged  = false;  // false = 0; true = any non-zero number
byte stateCurrent    = STATE_STOPPED;
byte stateOld        = stateCurrent;
bool stateChanged = false;

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
//const byte RS485_MAX_LEN     = 20;    // buffer length to hold the longest possible RS485 message.  Just a guess.
//      byte RS485MsgIncoming[RS485_MAX_LEN];  // No need to initialize contents
//      byte RS485MsgOutgoing[RS485_MAX_LEN];
//const byte RS485_LEN_OFFSET  =  0;    // first byte of message is always total message length in bytes
//const byte RS485_TO_OFFSET   =  1;    // second byte of message is the ID of the Arduino the message is addressed to
//const byte RS485_FROM_OFFSET =  2;    // third byte of message is the ID of the Arduino the message is coming from
// Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
//const byte RS485_TRANSMIT    = HIGH;  // HIGH = 0x1.  How to set TX_CONTROL pin when we want to transmit RS485
//const byte RS485_RECEIVE     = LOW;   // LOW = 0x0.  How to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

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
const byte         FRAM1VERSION[3]          = { 12, 02, 17 };  // This must match the version date stored in the FRAM1 control block.
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
const byte         FRAM1_PARK1_REC_LEN      = 127;  // Was 118 until 12/3/17
const byte         FRAM1_PARK1_RECS         =  19;
const byte         FRAM1_RECS_PER_PARK1     =  21;  // Max number of block/turnout records in a single Park 1 route in the route table.
const unsigned int FRAM1_PARK2_START        = FRAM1_PARK1_START + (FRAM1_PARK1_REC_LEN * FRAM1_PARK1_RECS);
const byte         FRAM1_PARK2_REC_LEN      =  52;  // Was 43 until 12/3/17
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

// *** MISC CONSTANTS AND GLOBALS: needed by A-LED:
// true = any non-zero number
// false = 0

// Set MAX_TURNOUTS_TO_BUF to be the maximum number of turnout commands that will potentially pile up coming from A-MAS RS485
// i.e. could be several routes being assigned in rapid succession when Auto mode is started.  Longest "regular" route has 8 turnouts,
// so conceivable we could get routes for maybe 10 trains = 80 records to buffer, maximum ever conceivable.
// If we overflow the buffer, we can easily increase the size by increasing this variable.
// We don't need to worry about multiple Park routes, because those are executed one at a time.
const byte MAX_TURNOUTS_TO_BUF            =  80;  // How many turnout commands might pile up before they can be executed?
const unsigned long TURNOUT_ACTIVATION_MS = 110;  // How many milliseconds to hold turnout solenoids before releasing.
const byte TOTAL_TURNOUTS                 =  32;  // Used to dimension our turnout_no/relay_no cross reference table.  30 connected, but 32 relays.
const byte LED_DARK                       =   0;  // LED off
const byte LED_GREEN_SOLID                =   1;  // Green turnout indicator LED lit solid
const byte LED_GREEN_BLINKING             =   2;  // Green turnout indicator LED lit blinking
const unsigned long LED_FLASH_MS          = 500;  // Toggle "conflicted" LEDs every 1/2 second
const unsigned long LED_REFRESH_MS        = 100;  // Refresh LEDs on control panel every 50ms, just so it isn't constant

// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// *** ROUTE REFERENCE TABLES...
// Note 9/12/16 added maxTrainLen which is typically the destination siding length
// A-LED needs route information for when it sees commands to set all turnouts for a given route (regular, park1, or park2.)
// Other times, A-LED will simply see a command to set an individual turnout to either Normal or Reverse, or a bit string.
struct routeReference {                 // Each element is 77 bytes, 26 or 70 records (single or double level)
  byte routeNum;                        // Redundant, but what the heck.
  char originSiding[5];                 // 4 chars + null terminator = 5 bytes
  byte originTown;                      // 1 byte
  char destSiding[5];                   // 4 chars + null terminator = 5 bytes
  byte destTown;                        // 1 byte
  byte maxTrainLen;                     // In inches, typically the destination siding length.  1 byte
  char restrictions[6];                 // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down.  1 byte
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
  char route[FRAM1_RECS_PER_ROUTE][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_ROUTE per route, each with 4 chars (plus \0).  55 bytes
};
routeReference routeElement;            // Use this to hold individual elements when retrieved

struct park1Reference {                 // Each element is 127 bytes, total of 19 records = 2413 bytes.
  byte routeNum;                        // Redundant, but what the heck.
  char originSiding[5];                 // 4 chars + null terminator = 5 bytes
  byte originTown;                      // 1 byte
  char destSiding[5];                   // 4 chars + null terminator = 5 bytes
  byte destTown;                        // 1 byte
  byte maxTrainLen;                     // In inches, typically the destination siding length.  1 byte
  char restrictions[6];                 // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate
  char route[FRAM1_RECS_PER_PARK1][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_PARK1 per route, each with 4 chars (plus \0)
};
park1Reference park1Element;            // Use this to hold individual elements when retrieved

struct park2Reference {                 // Each record 52 bytes long, total of just 4 records = 208 bytes
  byte routeNum;                        // Redundant, but what the heck.
  char originSiding[5];                 // 4 chars + null terminator = 5 bytes
  byte originTown;                      // 1 byte
  char destSiding[5];                   // 4 chars + null terminator = 5 bytes
  byte destTown;                        // 1 byte
  byte maxTrainLen;                     // In inches, typically the destination siding length.  1 byte
  char restrictions[6];                 // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;                     // Entry sensor number of the dest siding - where we start slowing down
  byte exitSensor;                      // Exit sensor number of the dest siding - where we do a Stop Immediate
  char route[FRAM1_RECS_PER_PARK2][5];  // Blocks and Turnouts, up to FRAM1_RECS_PER_PARK2 per route, each with 4 chars (plus \0)
};
park2Reference park2Element;  // Use this to hold individual elements when retrieved

// *** TURNOUT COMMAND BUFFER...
// 09/19/17: Re-wrote per new circular buffer logic.
// Create a circular buffer to store incoming RS485 "set turnout" commands from A-MAS.
// This structure only stores discrete turnout commands, not Route, Park 1, Park 2, or Last-known "group" commands.
byte turnoutCmdBufHead = 0;    // Next array element to be written.
byte turnoutCmdBufTail = 0;    // Next array element to be removed.
byte turnoutCmdBufCount = 0;   // Num active elements in buffer.  Max is MAX_TURNOUTS_TO_BUF.
struct turnoutCmdBufStruct {
  char turnoutDir;             // 'N' for Normal, 'R' for Reverse
  byte turnoutNum;             // 1..TOTAL_TURNOUTS
};
turnoutCmdBufStruct turnoutCmdBuf[MAX_TURNOUTS_TO_BUF];
char turnoutDirSingle;         // Globals to hold a single command/number pair.  'N' or 'R'
byte turnoutNumSingle;         // 1..32

// *** TURNOUT DIRECTION CURRENT STATUS...
// 10/25/16: A-LED only.  Populated with current turnout positions/orientations.
// Record corresponds to [turnout number - 1] = (0..31).  I.e. turnout #1 at array position zero.
// turnoutPosition[0..29] = 'N', 'R', or blank if unknown (at init).  We will give it 32 elements for future expansion.
char turnoutDir[TOTAL_TURNOUTS] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};

// *** TURNOUT LED DESIRED STATUS...
// 10/25/16: A-LED only.  Populated with what the status of each green LED *should* be, based on above turnout positions.
// Record number corresponds to Centipede shift register output pin (0..63)
// turnoutLEDStatus[0..63] = 0:off, 1:on, 2:blinking
// const byte LED_DARK = 0;                  // LED off
// const byte LED_GREEN_SOLID = 1;           // Green turnout indicator LED lit solid
// const byte LED_GREEN_BLINKING = 2;        // Green turnout indicator LED lit blinking
// The following zeroes should really be {LED_DARK, LED_DARK, LED_DARK, ... } but just use zeroes here for convenience

byte turnoutLEDStatus[TOTAL_TURNOUTS * 2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// *** TURNOUT LED SHIFT REGISTER PIN NUMBERS...
// 10/25/16: A-LED only.  Cross-reference array that gives us a Centipede shift register bit number 0..63 for a corresponding
// turnout indication i.e. 17N or 22R.  This is just how we chose to wire which button to each Centipede I/O port.
// Note that when we illuminate one green turnout indicator LED, we must darken its counterpart since a turnout
// can only be thrown one way or the other, not both.  Or blink when an LED is shared by two turnouts and their orientations conflict.
// Here is the "Map" of which turnout position LEDs are connected to which outputs on the Centipedes:
// 1N = Centipede output 0      1R = Centipede output 16
// ...                          ...
// 16N = Centipede output 15    16R = Centipede output 31
// 17N = Centipede output 32    17R = Centipede output 48
// ...                          ...
// 32N = Centipede output 47    32R = Centipede output 63
//
// And here is a cross-reference of the possibly conflicting turnouts/orientations and their Centipede pin numbers:
// 1R/3R are pins 16 & 18
// 2R/5R are pins 17 & 20
// 6R/7R are pins 21 & 22
// 2N/3N are pins 1 & 2
// 4N/5N are pins 3 & 4
// 12N/13R are pins 11 & 28
// 17N/18R are pins 32 & 49
// 27N/28R are pins 42 & 59
// If one of the above pair is "on" and the other is "off", then both pins (which are connected together) should be set to blink.

// Update turnoutLEDStatus[0..63] for each LED using this cross reference, which provides Centipede shift register pin number for each turnout+orientation:
const byte LEDNormalTurnoutPin[TOTAL_TURNOUTS] = 
   { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
const byte LEDReverseTurnoutPin[TOTAL_TURNOUTS] =
   {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

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

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // See if we have an incoming RS485 message...
  if (RS485GetMessage(RS485MsgIncoming)) {   // If returns true, then we got a complete RS485 message (may or may not be for us)

    // Need to reset the state and mode changed flags so we don't execute special code more than once...
    stateChanged = false;
    modeChanged = false;

    // CHECK FOR MODE/STATE-CHANGE MESSAGE AND HANDLE FOR ALL MODES...
    if (RS485fromMAStoALL_ModeMessage()) {  // If we just received a mode/state change message from A-MAS
      RS485UpdateModeState(&modeOld, &modeCurrent, &modeChanged, &stateOld, &stateCurrent, &stateChanged);
    }

    if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_SWT) {
      // The incoming message was for us!  It will either be to set a Turnout or a Route or a sting of bits for "last-known" from A-MAS
      // Byte #3 of RS485MsgIncoming can be 'N' or 'R', for Normal or Revers with a turnout number, or 'T' for rouTe with a route number,
      // (or 1 for Park 1 route, or 2 for Park 2 route),
      // or 'L' for "set to Last-known turnout positions" followed by a 4-byte = 32-bit data of 0=Normal, 1=Reverse, and each bit corresponds
      // to the turnout number in question, starting with bit 0 = turnout #1.
      // i.e. 'R08' = Turnout #8 reverse
      // i.e. 'N17' = Turnout #17 normal
      // i.e. 'T04' = rouTe #4
      // i.e. 'L2395' = Set all 32 turnouts as indicated by bit pattern 2395.

      // *** IS THIS A SINGLE TURNOUT COMMAND i.e. "Set turnout #14 to Reverse"?
      if ((RS485MsgIncoming[3] == 'N') || (RS485MsgIncoming[3] == 'R')) {    // Discrete Normal|Reverse turnout command
        sprintf(lcdString, "Received: %2i %c", RS485MsgIncoming[4], RS485MsgIncoming[3]);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        // Add the turnout command to the circular buffer for later processing...
        turnoutCmdBufEnqueue(RS485MsgIncoming[3], RS485MsgIncoming[4]);

      // *** IS THIS A ROUTE (not PARK1 or PARK2) COMMAND i.e. "Set all turnouts to be aligned for Route 47"
      } else if (RS485MsgIncoming[3] == 'T') {    // rouTe command, so create a bunch of turnout commands...
        byte routeNum = RS485MsgIncoming[4];
        sprintf(lcdString, "Route: %2i", RS485MsgIncoming[4]);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        // Retrieve route number "routeNum" from Route Reference table (FRAM1) and create a new record in the
        // turnout command buffer for each turnout in the route...
        unsigned long FRAM1Address = FRAM1_ROUTE_START + ((routeNum - 1) * FRAM1_ROUTE_REC_LEN); // Rec # vs Route # offset by 1
        byte b[FRAM1_ROUTE_REC_LEN];  // create a byte array to hold one Route Reference record
        FRAM1.read(FRAM1Address, FRAM1_ROUTE_REC_LEN, b);  // (address, number_of_bytes_to_read, data
        memcpy(&routeElement, b, FRAM1_ROUTE_REC_LEN);
        for (byte j = 0; j < FRAM1_RECS_PER_ROUTE; j++) {   // There are up to FRAM1_RECS_PER_ROUTE turnout|block commands per route, look at each one
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
            turnoutCmdBufEnqueue(d, n);
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
            turnoutCmdBufEnqueue(d, n);
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
            turnoutCmdBufEnqueue(d, n);
          } else if ((park2Element.route[j][0] != 'B') && (park2Element.route[j][0] != ' ')) {   // Not T, B, or blank = error
            sprintf(lcdString, "%.20s", "ERR bad park2 rec!");
            sendToLCD(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
          }
        }

      } else if (RS485MsgIncoming[3] == 'L') {    // Last-known-orientation command, so create a turnout command for each bit in next 4 bytes...
        // i.e. 'L2395' = Set all 32 turnouts as indicated by bit pattern 2395.
        // Read each of 32 bits and populate the turnout command circular buffer with 32 new records.
        sprintf(lcdString, "%.20s", "Set last-known-route");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 0; i < 4; i++) {   // i represents each of the four bytes of turnout data
          for (byte j = 0; j < 8; j++) {  // j represents each bit of a byte
            byte k = (i * 8) + j + 1;         // k represents turnout number (1..32)
            if (bitRead(RS485MsgIncoming[4 + i], j) == 0) {  // Bit is zero, so set turnout Normal
              turnoutCmdBufEnqueue('N', k);
            } else {                                       // Bit must be a 1, so set turnout Reverse
              turnoutCmdBufEnqueue('R', k);
            }
          }
        }
      
      } else {   // Fatal error if none of the above!
            sprintf(lcdString, "%.20s", "ERR bad turnout rec!");
            sendToLCD(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
      }  // End of handing this incoming RS485 record, that was for us.

    }   // End of "if it's for us"; otherwise msg wasn't for A-SWT, so just ignore (i.e.  no 'else' needed.)

  }  // end of "if RS485 serial available" block.  We may or may not have added record(s) to the turnout command buffer.

  else {   // We won't even attempt to call turnoutCmdBufProcess() unless the incoming RS485 buffer is empty.

    turnoutCmdBufProcess();  // execute a command from the turnout buffer, if any

  }

  updateLEDs();   // Refresh the green LEDs on the control panel

}  // end of main loop()


// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

void initializePinIO() {
  // Rev 7/14/16: Initialize all of the input and output pins
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
  return;
}

bool turnoutCmdBufIsEmpty() {
  // Rev: 09/29/17
  return (turnoutCmdBufCount == 0);
}

bool turnoutCmdBufIsFull() {
  // Rev: 09/29/17
  return (turnoutCmdBufCount == MAX_TURNOUTS_TO_BUF);
}

void turnoutCmdBufEnqueue(const char tTurnoutDir, const byte tTurnoutNum) {
  // Rev: 09/29/17.  Insert a record at the head of the turnout command buffer, then increment head and count.
  // If the buffer is already full, trigger a fatal error and terminate.
  // Although the two passed parameters are global, we are passing them for clarity.
  if (!turnoutCmdBufIsFull()) {
    turnoutCmdBuf[turnoutCmdBufHead].turnoutDir = tTurnoutDir;  // Store the orientation Normal or Reverse
    turnoutCmdBuf[turnoutCmdBufHead].turnoutNum = tTurnoutNum;  // Store the turnout number 1..TOTAL_TURNOUTS
    turnoutCmdBufHead = (turnoutCmdBufHead + 1) % MAX_TURNOUTS_TO_BUF;
    turnoutCmdBufCount++;
  } else {
    Serial.println("FATAL ERROR!  Turnout buffer overflow.");
    sprintf(lcdString, "%.20s", "Turnout buf ovrflw!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  return;
}

bool turnoutCmdBufDequeue(char * tTurnoutDir, byte * tTurnoutNum) {
  // Rev: 09/29/17.  Retrieve a record, if any, from the turnout command buffer.
  // If the turnout command buffer is not empty, retrieves a record from the buffer, clears it, and puts the
  // retrieved data into the called parameters.
  // Returns 'false' if buffer is empty, and the passed parameters remain undefined.  Not fatal.
  // Data will be at tail, then tail will be incremented (modulo size), and count will be decremented.
  // Because this function returns values via the passed parameters, CALLS MUST SEND THE ADDRESS OF THE RETURN VARIABLES, i.e.:
  //   status = turnoutCmdBufDequeue(&turnoutDirSingle, &turnoutNumSingle);
  // And because we are passing addresses, our code inside this function must use the "*" dereference operator.
  if (!turnoutCmdBufIsEmpty()) {
    * tTurnoutDir = turnoutCmdBuf[turnoutCmdBufTail].turnoutDir;
    * tTurnoutNum = turnoutCmdBuf[turnoutCmdBufTail].turnoutNum;
    turnoutCmdBufTail = (turnoutCmdBufTail + 1) % MAX_TURNOUTS_TO_BUF;
    turnoutCmdBufCount--;
    return true;
  } else {
    return false;  // Turnout command buffer is empty
  }
}

void turnoutCmdBufProcess() {   // Special version for A-LED, not the same as used by A-SWT
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
  
  if ((millis() - recTimeProcessed) > TURNOUT_ACTIVATION_MS) {   // This is true approx every 1/10 second
    // Performs timer check before checking if a record is available, so we don't light a whole route instantly.

    if  (turnoutCmdBufDequeue(&turnoutDirSingle, &turnoutNumSingle)) {
      // If we get here, we have a new turnout to "throw" *and* it's after a delay.
      recTimeProcessed = millis();  // refresh timer for delay between checking for "throws"

      // Each time we have a new turnout to throw, we need to update TWO arrays:
      // 1: Update turnoutPosition[0..31] for the turnout that was just "thrown."  So we have a list of all current positions.
      // 2: Update turnoutLEDStatus[0..63] to indicate how every green LED *should* now look, with conflicts resolved.

      // First tell the operator what we are going to do...
      if (turnoutDirSingle == 'N') {
        sprintf(lcdString, "Throw %2i N", turnoutNumSingle);
        sendToLCD(lcdString);
        Serial.println(lcdString);
      } else if (turnoutDirSingle == 'R') {
        sprintf(lcdString, "Throw %2i R", turnoutNumSingle);
        sendToLCD(lcdString);
        Serial.println(lcdString);
      } else {
        sprintf(lcdString, "Turnout buf error!");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        endWithFlashingLED(3);   // error!
      }

      // For example, we might update turnoutPosition[7] = 'R' to indicate turnout #7 is now in Reverse orientation
      turnoutDir[turnoutNumSingle - 1] = turnoutDirSingle;  // 'R' and 'N' are the only valid possibilities

      // Now that we have revised our "turnoutPosition[0..31] = N or R" array, we should fully re-populate our turnoutLEDStatus[] array
      // that indicates which green turnout LEDs should be on, off, or blinking -- taking into account "conflicts."
      // There are 8 special cases, where we have an LED that is shared by two turnouts.  If the LED should bit lit based
      // on one of the pair, but off according to the other turnout, then the shared LED should be blinking.
      // This means that if two facing turnouts, that share an LED, are aligned with each other, then the LED will be solid.
      // If those two turnouts are BOTH aligned away from each other, then the LED will be dark.
      // Finally if one, but not both, turnout is aligned with the other, then the LED will blink.

      // First update the "lit" status of the newly-acquired turnout LED(s) to illuminate Normal or Revers, and turn off it's opposite.
      // This is done via the turnoutLEDStatus[0..63] array, where each element represents an LED, NOT A TURNOUT NUMBER.
      // The offset to the LED number into the turnoutLEDStatus[0..63] array is found in the LEDNormalTurnoutPin[0..31] (for turnouts 1..32 'Normal')
      // and LEDReverseTurnoutPin[0..31] (for turnouts 1..32 'Reverse') arrays, which are defined as constant byte arrays at the top of the code.
      // I.e. suppose we just read Turnout #7 is now set to Reverse:
      // Look up LEDNormalTurnoutPin[6] (turnout #7) =  Centipede pin 6, and LEDReverseTurnoutPin[6] (also turnout #7) = Centipede pin 22.
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
      // The above "map" is defined in the LEDNormalTurnoutPin[0..31] and LEDReverseTurnoutPin[0..31] arrays:
      // const byte LEDNormalTurnoutPin[TOTAL_TURNOUTS] = 
      //   { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
      // const byte LEDReverseTurnoutPin[TOTAL_TURNOUTS] =
      //   {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

      for (byte i = 0; i < TOTAL_TURNOUTS; i++) {
        if (turnoutDir[i] == 'N') {
          turnoutLEDStatus[LEDNormalTurnoutPin[i]] = LED_GREEN_SOLID;
          turnoutLEDStatus[LEDReverseTurnoutPin[i]] = LED_DARK;
        } else if (turnoutDir[i] == 'R') {
          turnoutLEDStatus[LEDNormalTurnoutPin[i]] = LED_DARK;
          turnoutLEDStatus[LEDReverseTurnoutPin[i]] = LED_GREEN_SOLID;
        }
      }

      // Now we need to identify any conflicting turnout orientations, and set the appropriate turnoutLEDStatus[]'s to 2 = blinking.
      // Here is a cross-reference of the possibly conflicting turnouts/orientations and their Centipede shift register pin numbers.
      // Conflicting means when facing turnouts are not either *both* aligned with each other, or *both* aligned against the other.
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

      if ((turnoutDir[0] == 'R') && (turnoutDir[2] == 'N')) {   // Remember turnoutPosition[0] refers to turnout #1
        turnoutLEDStatus[16] = LED_GREEN_BLINKING;
        turnoutLEDStatus[18] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[0] == 'N') && (turnoutDir[2] == 'R')) {   // Remember turnoutPosition[0] refers to turnout #1
        turnoutLEDStatus[16] = LED_GREEN_BLINKING;
        turnoutLEDStatus[18] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[1] == 'R') && (turnoutDir[4] == 'N')) {
        turnoutLEDStatus[17] = LED_GREEN_BLINKING;
        turnoutLEDStatus[20] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[1] == 'N') && (turnoutDir[4] == 'R')) {
        turnoutLEDStatus[17] = LED_GREEN_BLINKING;
        turnoutLEDStatus[20] = LED_GREEN_BLINKING;
      }
 
      if ((turnoutDir[5] == 'R') && (turnoutDir[6] == 'N')) {
        turnoutLEDStatus[21] = LED_GREEN_BLINKING;
        turnoutLEDStatus[22] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[5] == 'N') && (turnoutDir[6] == 'R')) {
        turnoutLEDStatus[21] = LED_GREEN_BLINKING;
        turnoutLEDStatus[22] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[1] == 'N') && (turnoutDir[2] == 'R')) {
        turnoutLEDStatus[1] = LED_GREEN_BLINKING;
        turnoutLEDStatus[1] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[1] == 'R') && (turnoutDir[2] == 'N')) {
        turnoutLEDStatus[1] = LED_GREEN_BLINKING;
        turnoutLEDStatus[1] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[3] == 'N') && (turnoutDir[4] == 'R')) {
        turnoutLEDStatus[3] = LED_GREEN_BLINKING;
        turnoutLEDStatus[4] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[3] == 'R') && (turnoutDir[4] == 'N')) {
        turnoutLEDStatus[3] = LED_GREEN_BLINKING;
        turnoutLEDStatus[4] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[11] == 'N') && (turnoutDir[12] == 'N')) {
        turnoutLEDStatus[11] = LED_GREEN_BLINKING;
        turnoutLEDStatus[28] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[11] == 'R') && (turnoutDir[12] == 'R')) {
        turnoutLEDStatus[11] = LED_GREEN_BLINKING;
        turnoutLEDStatus[28] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[16] == 'N') && (turnoutDir[17] == 'N')) {
        turnoutLEDStatus[32] = LED_GREEN_BLINKING;
        turnoutLEDStatus[49] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[16] == 'R') && (turnoutDir[17] == 'R')) {
        turnoutLEDStatus[32] = LED_GREEN_BLINKING;
        turnoutLEDStatus[49] = LED_GREEN_BLINKING;
      }

      if ((turnoutDir[26] == 'N') && (turnoutDir[27] == 'N')) {
        turnoutLEDStatus[42] = LED_GREEN_BLINKING;
        turnoutLEDStatus[59] = LED_GREEN_BLINKING;
      }
      if ((turnoutDir[26] == 'R') && (turnoutDir[27] == 'R')) {
        turnoutLEDStatus[42] = LED_GREEN_BLINKING;
        turnoutLEDStatus[59] = LED_GREEN_BLINKING;
      }
    }
  }
  return;
}

void updateLEDs() {
  // Rev 10/23/16: Based on the array of current turnout status, update every green turnout-indicator LED on the control panel.
  // Note that although there are not actually two LEDs for every turnout - since facing turnouts share an LED - we have 64
  // outputs wired, so we can code as if there are 64 LEDs.
  // Any facing turnouts THAT ARE NOT IN SYNC will have their shared LED blinking.  Otherwise, just turn each green LED on or
  // off, as appropriate.
  // We will update all green LEDs after every LED_REFRESH_MS number of milliseconds passes i.e every 1/10 second or whatever.
  // However we will only flash them at a rate of toggling every LED_FLASH_MS milliseconds i.e. every 1/2 second or so.
  // When we have facing turnouts that may not be aligned with each other (i.e. "conflicted",) we want the dual-role LED to
  // blink, rather than be on or off.  I can't think of any other way to indicate those cases.

  // At this point, we should already have: turnoutLEDStatus[0..63] = 0 (off), 1 (on), or 2 (blinking)
  // So now just update the physical LEDs on the control panel.
  // const int LED_FLASH_MS = 500;           // Toggle "conflicted" LEDs every 1/2 second
  // const int LED_REFRESH_MS = 100;         // Refresh LEDs on control panel every 50ms, just so it isn't constant

  // We have two delay periods, because we want the flashing LEDs to toggle on/off only every 1/2 second or so, but when we
  // are updating the LEDs because a bunch of turnouts are being thrown, we want to turn them on or off at the same rate
  // that they are physically being thrown by A-SWT -- about every 110ms at this point.  I.e. faster than the blink rate.
  static unsigned long LEDRefreshProcessed = millis(); // Delay between updating the LEDs on the control panel i.e. 1/10 second.
  static unsigned long LEDFlashProcessed = millis();   // Delay between toggling flashing LEDs on control panel i.e. 1/2 second.
  static bool LEDsOn = false;    // Toggle this variable to know when conflict LEDs should be on versus off.  NOT USED YET
  
  if ((((millis() - LEDRefreshProcessed) > LED_REFRESH_MS))) {
    // If we get here, then we should refresh the status of the LEDs on the control panel in case turnoutPosition[] has changed.
    LEDRefreshProcessed = millis();  // Reset "refresh delay" timer.  HAVE NOT USED THIS YET.
    
    // Write a ZERO to a bit to turn on the LED, write a ONE to a bit to turn the LED off.  Opposite of our turnoutLEDStatus[] array.

    for (byte i = 0; i < TOTAL_TURNOUTS * 2; i++) {    // For every "bit" of the Centipede shift register output
      if (stateCurrent == STATE_STOPPED) {   // Darken all green LEDs when in ANY mode, if Stopped
        shiftRegister.digitalWrite(i, HIGH);       // turn off the LED
      } else if (modeCurrent == MODE_REGISTER) {  // Darken all green LEDs when in Register mode, any state
        shiftRegister.digitalWrite(i, HIGH);       // turn off the LED
      } else {   // mode is not MODE_REGISTER, and state is either STATE_RUNNING or STATE_STOPPING, so okay to illuminate LEDs
        if (turnoutLEDStatus[i] == LED_DARK) {   // LED should be off
          shiftRegister.digitalWrite(i, HIGH);       // turn off the LED
        } else if (turnoutLEDStatus[i] == LED_GREEN_SOLID) {   // LED should be on
          shiftRegister.digitalWrite(i, LOW);        // turn on the LED
        } else {                          // Must be 2 = LED_GREEN_BLINKING
          if (LEDsOn) {
            shiftRegister.digitalWrite(i, LOW);        // turn on the LED until we have code to handle this
          } else {
            shiftRegister.digitalWrite(i, HIGH);        // turn on the LED until we have code to handle this
          }
        }
      }
    }
  }

  if ((millis() - LEDFlashProcessed) > LED_FLASH_MS) {
    LEDFlashProcessed = millis();   // Reset flash timer (about every 1/2 second)
    LEDsOn = !LEDsOn;    // Invert flash toggle
  }
  return;
}

void initializeShiftRegisterPins() {
  // Rev 9/14/16: Set all chips on Centipede shift register to output, high (i.e. turn off all LEDs)
  for (int i = 0; i < 4; i++) {         // For each of 4 chips per board / 1 board = 4 chips @ 16 bits/chip
    // Set outputs high before setting pin mode to output, to prevent brief low being sent to Centipede shift register outputs
    shiftRegister.portWrite(i, 0b1111111111111111);  // Set all outputs HIGH (pull LOW to turn on an LED)
    shiftRegister.portMode(i, 0b0000000000000000);   // Set all pins on this chip to OUTPUT
  }
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

