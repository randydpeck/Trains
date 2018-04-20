// A_SWT Rev: 04/19/18.
char APPVERSION[21] = "A-SWT Rev. 04/19/18";

// Include the following #define if we want to run the system with just the lower-level track.
#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

// A-SWT controls turnouts (switch machine) via Centipede shift register and 16-Relay boards.
// This is an "INPUT-ONLY" module that does not provide data to any other Arduino.

// IMPORTANT: LARGE AMOUNTS OF THIS CODE ARE IDENTIAL IN A-LED, SO ALWAYS UPDATE A-LED WHEN WE MAKE CHANGES TO THIS CODE.
// 04/19/18: Lots of changes to use new const, message, and 2004 LCD objects.
// 09/16/17: Changed variable suffixes from Length to Len, No/Number to Num, Record to Rec, etc.
// 11/01/16: checkIfHaltPulledLow(), if it was and we needed to halt, was not releasing the relays and thus could lock ON a solenoid!
// 11/01/16: Also re-wrote watchdog timer to release relays under any circumstances if left on for more than 1/2 second or whatever.

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

// **************************************************************************************************************************

// *** RS485 MESSAGE PROTOCOLS used by A-SWT  (similar to A-LED.)  Byte numbers represent offsets, so they start at zero. ***
// Rev: 11/06/17
// UPDATE 4/19/18: I don't think A_SWT cares about Mode change commands coming from A_MAS, so deleted it from this list...
// We're going to do whatever A_MAS tells us to do (i.e. throw a turnout) no matter the mode, because that's up to A_MAS (based on mode.)

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
const byte THIS_MODULE = ARDUINO_SWT;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?

#include <avr/wdt.h>     // Required to call wdt_reset() for watchdog timer for turnout solenoids



// *** SERIAL LCD DISPLAY CLASS:
#include <Display_2004.h>                // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
// Instantiate a Display_2004 object called "LCD2004".
// Pass address of serial port to use (0..3) such as &Serial1, and baud rate such as 9600 or 115200.
Display_2004 LCD2004(&Serial1, 115200);  // Instantiate 2004 LCD display "LCD2004."
Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.
char lcdString[LCD_WIDTH + 1];           // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** MESSAGE CLASS (RS485 and digital pin communications):
#include <Message_SWT.h>                 // Class includes all messages sent and received by A_SWT, including parent messages.
Message_SWT Message(ptrLCD2004);         // Instantiate message object "Message"; requires a pointer to the 2004 LCD display
byte MsgIncoming[RS485_MAX_LEN];         // Global array for incoming inter-Arduino messages.  No need to init contents.  Probably shouldn't call them "RS485" though.
// byte MsgOutgoing[RS485_MAX_LEN];    No need to initialize contents.  Also, A_SWT doesn't send any messages, not even digital lines.

// *** SHIFT REGISTER CLASS:
#include <Wire.h>                        // Needed for Centipede shift register.
#include <Centipede.h>                   // Also, obviously, needed for Centipede.
Centipede shiftRegister;                 // Instantiate Centipede shift register object "shiftRegister".

// *** FRAM MEMORY MODULE:  Constants and variables needed for use with Hackscribble Ferro RAM.
// FRAM is used to store the Route Reference Table, the Delayed Action Table, and other data.
// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
// IMPORTANT: 7/12/16: Modified FRAM library to change max buffer size from 64 bytes to 128 bytes.
// As of 10/18/16, we are using the standard libarary part no. MB85RS64, not the actual part no. MB85RS64V.
// So the only mod to the Hackscribble_Ferro library is changing max block size from 0x40 to 0x80.
// To create an instance of FRAM, we specify a name, the FRAM part number, and our SS pin number, i.e.:
//    Hackscribble_Ferro FRAM1(MB85RS64, PIN_FRAM1);
// Control buffer in each FRAM is first 128 bytes (address 0..127) reserved for any special purpose we want such as config info.
#include <SPI.h>                                    // FRAM uses SPI communications
#include <Hackscribble_Ferro.h>                     // FRAM library
const unsigned int FRAM_CONTROL_BUF_SIZE = 128;     // This defaults to 64 bytes in the library, but we modified it
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
// Use the following code if we need a second FRAM memory module:
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

// *** MISC CONSTANTS AND GLOBALS: needed by A-SWT:
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
/*
#include <avr/wdt.h>     // Required to call wdt_reset() for watchdog timer for turnout solenoids  MOVED TO SWT FNS VARS CONST
*/
// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// *** ROUTE REFERENCE TABLES...
// Note 9/12/16 added maxTrainLen which is typically the destination siding length
// A-SWT needs route information for when it gets commands to set all turnouts for a given route (regular, park1, or park2.)
// Other times, A-SWT will simply get a command to set an individual turnout to either Normal or Reverse, or a bit string.
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

bool turnoutClosed  = false;  // Keeps track if relay coil is currently energized, so we know to check if it should be released
unsigned long turnoutActivationTime = millis();  // Keeps track of *when* a turnout coil was energized

// *** TURNOUT CROSS REFERENCE...
// 10/25/16: Unique to A-SWT is a cross-reference array that gives a Centipede shift register bit number 0..127 for a corresponding
// turnout identification i.e. 17N or 22R.  This is just how we chose to wire which turnout to which relay.
// Even though this could have been done with one Centipede (64 bits for 32 turnouts,) we are using two
// Centipedes to allow for future expansion, and thus outputs range from 0 through 127.
// Relays that control turnouts 1..16 (1N/1R...16N/16R) are on Centipede #1 on pins 0..63
// Relays that control turnouts 17..32 (17N/17R...32N/32R) are on Centipede #2 on pins 64..127
// Array order is: 1N, 2N, 3N...30N, 31N, 32N,1R, 2R, 3R...30R, 31R, 32R.
// So the first 32 elements are for turnouts 1..32 Normal, the second 32 elements are for turnouts 1..31 Reverse.
byte turnoutCrossRef[(TOTAL_TURNOUTS * 2)] =
  {15,14,13,12,11,10, 9, 8,19,18,17,16,23,22,21,20,79,78,77,76,75,74,73,72,83,82,81,80,87,86,85,84,    // Normals
    0, 1, 2, 3, 4, 5, 6, 7,28,29,30,31,24,25,26,27,64,65,66,67,68,69,70,71,92,93,94,95,88,89,90,91};   // Reverses
  
// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485  up to 115200
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // This ensures NO relays (thus turnout solenoids) are turned on (which would burn out solenoids)
  initializePinIO();                    // Initialize all of the I/O pins
  LCD2004.init();                       // Initialize the 20 x 04 Digole serial LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  initializeFRAM1AndGetControlBlock();  // Check FRAM1 version, and get control block
  sprintf(lcdString, "FRAM1 Rev. %02i/%02i/%02i", FRAM1GotVersion[0], FRAM1GotVersion[1], FRAM1GotVersion[2]);  // FRAM1 version no. on LCD display
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  wdtSetup();                           // Set up the watchdog timer to prevent solenoids from burning out
  
}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // See if we have an incoming RS485 message...
//  if (Message.RS485GetMessage(MsgIncoming)) {   // If returns true, then we got a complete RS485 message (may or may not be for us)
  if (Message.Receive(MsgIncoming)) {   // If returns true, then we got a complete RS485 message (may or may not be for us)
//    if (MsgIncoming[RS485_TO_OFFSET] == ARDUINO_SWT) {
    if ((Message.GetTo(MsgIncoming)) == ARDUINO_SWT) {
      // The incoming message was for us!  It will either be to set a Turnout or a Route or a sting of bits for "last-known" from A-MAS.
      // Byte #3 of MsgIncoming can be 'N' or 'R', for Normal or Revers with a turnout number, or 'T' (for rouTe) or '1' (for Park 1) or
      // '2' (for Park 2) with a route number, or 'L' for "set to Last-known turnout positions" followed by a 4-byte = 32-bit data of
      // 0=Normal, 1=Reverse, and each bit corresponds to the turnout number in question (offset by 1), starting with bit 0 = turnout #1.
      // i.e. 'R08' = Turnout #8 reverse
      // i.e. 'N17' = Turnout #17 normal
      // i.e. 'T04' = rouTe #4
      // i.e. '103' = Park 1 route #3
      // i.e. 'L2395' = Set all 32 turnouts as indicated by bit pattern 2395.  Each byte treated as independent 8 bits.

      // *** IS THIS A SINGLE TURNOUT COMMAND i.e. "Set turnout #14 to Reverse"?
//      if ((MsgIncoming[3] == 'N') || (MsgIncoming[3] == 'R')) {    // Discrete Normal|Reverse turnout command
      if ((Message.GetType(MsgIncoming) == 'N') || (Message.GetType(MsgIncoming) == 'R')) {    // Discrete Normal|Reverse turnout command
        sprintf(lcdString, "Received: %2i %c", MsgIncoming[4], MsgIncoming[3]);
        LCD2004.send(lcdString);
        Serial.println(lcdString);
        // Add the turnout command to the circular buffer for later processing...
        turnoutCmdBufEnqueue(MsgIncoming[3], MsgIncoming[4]);

      // *** IS THIS A ROUTE (not PARK1 or PARK2) COMMAND i.e. "Set all turnouts to be aligned for Route 47"
//      } else if (MsgIncoming[3] == 'T') {    // rouTe command, so create a bunch of turnout commands...
      } else if (Message.GetType(MsgIncoming) == 'T') {    // rouTe command, so create a bunch of turnout commands...
        byte routeNum = MsgIncoming[4];
        sprintf(lcdString, "Route: %2i", MsgIncoming[4]);
        LCD2004.send(lcdString);
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
              LCD2004.send(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
            sprintf(lcdString, "Route turnout %2i %c", n, d);
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            // Add this turnout command to the circular turnout command buffer...
            turnoutCmdBufEnqueue(d, n);
          } else if ((routeElement.route[j][0] != 'B') && (routeElement.route[j][0] != ' ')) {   // Not T, B, or blank = error
            sprintf(lcdString, "%.20s", "Bad turnout rec!");
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
          }
        }

//      } else if (MsgIncoming[3] == '1') {    // 'Park 1' route command, so create a bunch of turnout commands...
      } else if (Message.GetType(MsgIncoming) == '1') {    // 'Park 1' route command, so create a bunch of turnout commands...
        byte routeNum = MsgIncoming[4];
        sprintf(lcdString, "Park 1 route %2i", routeNum);
        LCD2004.send(lcdString);
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
              LCD2004.send(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
            sprintf(lcdString, "Park 1: %2i %c", n, d);
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            // Add this turnout command to the circular turnout command buffer...
            turnoutCmdBufEnqueue(d, n);
          } else if ((park1Element.route[j][0] != 'B') && (park1Element.route[j][0] != ' ')) {   // Not T, B, or blank = error
            sprintf(lcdString, "ERR bad prk1 element");
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
          }
        }
//      } else if (MsgIncoming[3] == '2') {    // 'Park 2' route command, so create a bunch of turnout commands...
      } else if (Message.GetType(MsgIncoming) == '2') {    // 'Park 2' route command, so create a bunch of turnout commands...
        byte routeNum = MsgIncoming[4];
        sprintf(lcdString, "Park 2 route %2i", routeNum);
        LCD2004.send(lcdString);
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
              LCD2004.send(lcdString);
              Serial.println(lcdString);
              endWithFlashingLED(3);
            }
            sprintf(lcdString, "Park 2: %2i %c", n, d);
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            // Add this turnout command to the circular turnout command buffer...
            turnoutCmdBufEnqueue(d, n);
          } else if ((park2Element.route[j][0] != 'B') && (park2Element.route[j][0] != ' ')) {   // Not T, B, or blank = error
            sprintf(lcdString, "%.20s", "ERR bad park2 rec!");
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
          }
        }
//      } else if (MsgIncoming[3] == 'L') {    // Last-known-orientation command, so create a turnout command for each bit in next 4 bytes...
      } else if (Message.GetType(MsgIncoming) == 'L') {    // Last-known-orientation command, so create a turnout command for each bit in next 4 bytes...
        // i.e. 'L2395' = Set all 32 turnouts as indicated by bit pattern 2395.
        // Read each of 32 bits and populate the turnout command circular buffer with 32 new records.
        sprintf(lcdString, "%.20s", "Set last-known-route");
        LCD2004.send(lcdString);
        Serial.println(lcdString);
        for (byte i = 0; i < 4; i++) {   // i represents each of the four bytes of turnout data
          for (byte j = 0; j < 8; j++) {  // j represents each bit of a byte
            byte k = (i * 8) + j + 1;         // k represents turnout number (1..32)
            if (bitRead(MsgIncoming[RS485_LAST_KNOWN_OFFSET + i], j) == 0) {  // Bit is zero, so set turnout Normal
              turnoutCmdBufEnqueue('N', k);
            } else {                                       // Bit must be a 1, so set turnout Reverse
              turnoutCmdBufEnqueue('R', k);
            }
          }
        }
      
      } else {   // Fatal error if none of the above!
            sprintf(lcdString, "%.20s", "ERR bad turnout rec!");
            LCD2004.send(lcdString);
            Serial.println(lcdString);
            endWithFlashingLED(3);
      }  // End of handing this incoming RS485 record, that was for us.

    }   // End of "if it's for us"; otherwise msg wasn't for A-SWT, so just ignore (i.e.  no 'else' needed.)

  }  // end of "if RS485 serial available" block.  We may or may not have added record(s) to the turnout command buffer.

  // Depending on whether or not a turnout solenoid is currently being activated, we want to either:
  //   1. If a relay is currently energized, see if it's time to release it yet, or
  //   2. Check if there is a new turnout to throw (from the turnout command buffer) and, if so, close a new relay.

  if (turnoutClosed) {  // A solenoid is being energized -- see if it's been long enough that we can release it
    // Keep the above and below "if" statements separate (not joined by &&), so we only do "else" if an energized solenoid gets released.
    if ((millis() - turnoutActivationTime) > TURNOUT_ACTIVATION_MS) {   // Long enough, we can release it!
      initializeShiftRegisterPins();   // Just release all relays, no need to identify the one that's activated
      turnoutClosed = false;  // No longer energized, happy days.
    }
  } else {   // Only if relays are all open can we check for a new turnout command and potentially close a new relay
    if (turnoutCmdBufProcess()) {    // Return of true means that we found a new turnout command in the buffer, and just energized a relay/solenoid...
      turnoutClosed = true;
      turnoutActivationTime = millis();
    }
  }

  // We know that if we get here, we just checked that if a turnout was closed, it was released if sufficient time has elapsed.
  // So this is a safe place to reset the watchdog counter.

  wdt_reset();

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

void initializeShiftRegisterPins() {
  // Rev 9/14/16: Set all chips on both Centipede shift registers to output, high (i.e. turn off relays on relay boards)
  for (int i = 0; i < 8; i++) {         // For each of 4 chips per board / 2 boards = 8 chips @ 16 bits/chip
    // Set outputs high before setting pin mode to output, to prevent brief low being sent to Centipede shift register outputs
    shiftRegister.portWrite(i, 0b1111111111111111);  // Set all outputs HIGH (pull LOW to turn on a relay)
    shiftRegister.portMode(i, 0b0000000000000000);   // Set all pins on this chip to OUTPUT
  }
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
    LCD2004.send(lcdString);
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

bool turnoutCmdBufProcess() {   // Special version for A-SWT, not the same as used by A-LED.
  // Rev: 09/19/17.
  // See if there is a record in the turnout command buffer, and if so, retrieve it and activate the relay/solenoid.
  // Only closes relay; does not release.  Returns true if a relay was energized, false if no action taken.
  if (turnoutCmdBufDequeue(&turnoutDirSingle, &turnoutNumSingle)) {   // Nothing previously activated, is there one to throw now?
    // If function returned true, we have a new turnout to throw!
    byte bitToWrite = 0;
    // Activate the turnout solenoid by turning on the relay coil connected to the Centipede shift register.
    // Use the cross-reference table I wrote for Relay Number vs Turnout N/R.
    // Write a ZERO to a bit to turn on the relay, otherwise all bits should be set to 1.
    // turnoutDirSingle will be either 'N' or 'R'
    // turnoutNumSingle will be turnout number 1..32
    if (turnoutDirSingle == 'N') {
      bitToWrite = turnoutCrossRef[turnoutNumSingle - 1];
      sprintf(lcdString, "Throw %2i N", turnoutNumSingle);
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    } else if (turnoutDirSingle == 'R') {
      bitToWrite = turnoutCrossRef[(turnoutNumSingle + 32) - 1];
      sprintf(lcdString, "Throw %2i R", turnoutNumSingle);
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    } else {
      sprintf(lcdString, "Turnout buf error!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(3);   // error!
    }
    shiftRegister.digitalWrite(bitToWrite, LOW);  // turn on the relay
    return true;    // So we will know that we need to turn it off
  } else {   // We did not retrieve a new turnout throw command and thus did not activate a relay
    return false;
  }
}

void wdtSetup() {
  // Rev: 09/14/16.  Set up a watchdog timer to run ISR(WDT_vect) every x seconds, unless reset with wdt_reset().
  // This is used to ensure that a relay (and thus a turnout solenoid) is not held "on" for more than x seconds.
  // This function uses a macro defined in wdt.h: #define _BV(BIT) (1<<BIT).  It just defines which bit to set in a byte
  // WDP3 WDP2 WDP1 WDP0 Time-out(ms)
  // 0    0    0    0    16
  // 0    0    0    1    32
  // 0    0    1    0    64
  // 0    0    1    1    125
  // 0    1    0    0    250
  // 0    1    0    1    500
  // 0    1    1    0    1000
  // 0    1    1    1    2000
  // 1    0    0    0    4000
  // 1    0    0    1    8000
  cli();  // Disables all interrupts by clearing the global interrupt mask.
  WDTCSR |= (_BV(WDCE) | _BV(WDE));   // Enable the WD Change Bit
  WDTCSR =   _BV(WDIE) |              // Enable WDT Interrupt
             _BV(WDP2) | _BV(WDP1);   // Set Timeout to ~1 seconds
  sei();  // Enables interrupts by setting the global interrupt mask.
  return;
}

ISR(WDT_vect) {
  // Rev 11/01/16: Watchdog timer.  Turn off all relays and HALT if we don't update the timer at least every 1/2 second.
  sei();  // This is required
  wdt_disable();  // Turn off watchdog timer or it will keep getting called.  Defined in wdt.h.
  initializeShiftRegisterPins();  // Release relays first, in case following serial commands blow up the code, even though also called by endWithFlashingLED().
  sprintf(lcdString, "%.20s", "Watchdog timeout!");
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  endWithFlashingLED(1);
}

// ***************************************************************************
// *** HERE ARE FUNCTIONS USED BY VIRTUALLY ALL ARDUINOS *** REV: 09-26-17 ***
// ***************************************************************************

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
    LCD2004.send(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(1);
  }

  if (FRAM1.getControlBlockSize() != 128) {
    sprintf(lcdString, "%.20s", "FRAM1 bad blk size.");
    LCD2004.send(lcdString);
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
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
  }

  // A-MAS will also retrieve last-known-turnout and last-known-train positions from control block, but nobody else needs this.
  return;
}

void endWithFlashingLED(int numFlashes) {
  // Rev 10/05/16: Version for Arduinos WITH relays that should be released (A-SWT turnouts, A-LEG accessories)
  initializeShiftRegisterPins();  // Release all relay coils that might be activating turnout solenoids

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
      initializeShiftRegisterPins();   // This will disable all relays/turnout solenoids.  VERY IMPORTANT!
      wdt_disable();  // No longer need WDT since relays are released and we are terminating program here.
      chirp();
      sprintf(lcdString, "%.20s", "HALT pin low!  End.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    } else {
      sprintf(lcdString, "False HALT detected.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}
