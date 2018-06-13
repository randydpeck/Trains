char APPVERSION[21] = "A-MAS Rev. 04/20/18";
#include <Train_Consts_Global.h>
const byte THIS_MODULE = ARDUINO_MAS;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?

// Include the following #define if we want to run the system with just the lower-level track.  Comment out to create records for both levels of track.
#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

// 04/22/18: Updating new global const, message and LCD display classes.
// 04/18/18: Changed line in RS485SendMessage back to original, as the bug was fixed by the vendor.
// 03/22/18: Changed line in RS485SendMessage to: "Serial2.write((unsigned char*) tMsg, tMsg[0]);" to fix red squiggly in Serial2.write(), per Tim M.
//           This seems to be a bug in an Arduino header file, so the fix may be eliminated at some point.  Also RS485SendMessage will get moved to 
//            a .h/.cpp file pair, so we may need to move the fix there, later.
// 03/21/18: Minor changes to get .h/.cpp files working.
// 10/01/17: Train Reference Table changed Legacy ID to a single byte since it can only range from 1..255 (Engine or Train.)
// 09/16/17: Changed variable suffixes from Length to Len, No/Number to Num, Record to Rec, etc.
// 03/15/17: Just a few items cleaned up, no change in functionality.
// 01/21/17: Adding reg code, to look at starting sensor status, get reg info from A-OCC via rot encoder, and starting Train Progress table.
// 01/16/17: Rearranged code above Setup() to be consistent with other modules.
// 11/11/16: Update code to reflect that in MANUAL, POV, and REGISTER modes, there is no STOPPING state.
// Go directly from RUNNING to STOPPED.  No point in STOPPING.
// 11/04/16: Display "set last-knonw" at startup.  Added delays in sendToLCD().
// 10/29/16: Adding some debug code to see why A-MAS sometimes hangs in Manual mode...
// 10/19/16: extended length of delay when checking if Halt pressed.
// 10/20/16 to change enum states to const bytes, because you can't easily pass enum values to functions.
// 10/19/16 by Randy.  Updated FRAM1 version number, a few other minor details.

// IMPORATANT TO-DO!!!!!
// ADD: Broadcast RS485 message of what mode we are in.  M, R, A, P, V.

// A_LEG certainly needs to know what mode it is running in.
// A_SNS probably doesn't care about mode -- it always wants to tell everyone when sensors change.
// A_BTN needs to know so that it will only try to advise of button presses when in Manual mode.
// A_SWT doesn't care what the mode is, it simply obeys commands from A_MAS.
// A_LED doesn't care what mode the system is in; it just updates turnout LEDs no matter the mode.
// A_OCC sort of needs to know mode, so that the "reserved" blocks (LEDs) will not be illuminated when
//       the operator switches to Manual mode.


// **************************************************************************************************************************





// ADD: TO MAS FROM BTN button update
// ADD: 





// *** RS485 MESSAGE PROTOCOLS used by A-MAS.  Byte numbers represent offsets, so they start at zero. ***
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

// A-MAS to A-BTN: Permission for A-BTN to send which turnout button was just pressed on the control panel
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//    0	  Length	  Byte	5
//    1	  To	      Byte	4 (A_BTN)
//    2	  From	    Byte	1 (A_MAS)
//    3	  Msg type	Char	'B' = Button request status (sent after it sees that a button was pressed, via a digital input from A-BTN.)
//    4	  Checksum	Byte  0..255

// A-BTN to A-MAS: Sending the number of the turnout button that was just pressed on the control panel
// Rev: 08/31/17
// OFFSET DESC      SIZE  CONTENTS
//    0	  Length	  Byte	6
//    1	  To	      Byte	1 (A_MAS)
//    2	  From	    Byte	4 (A_BTN)
//    3	  Msg type	Char	'B' = Button press update
//    4	  Button No.Byte  1..32
//    5	  Checksum	Byte  0..255

// A-MAS to A-SNS:  Permission for A_SNS to send a sensor change record.
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 5
//    1	   To	       Byte	 3 (A_SNS)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  'S' Sensor status request (sent after it sees that a sensor changed, via a digital input from A-SNS.)
//    4	   Checksum	 Byte  0..255

// A-SNS to A-MAS:  Sensor status update for a single sensor change
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 7
//    1	   To	       Byte	 1 (A_MAS)
//    2	   From	     Byte	 3 (A_SNS)
//    3    Command   Char  'S' Sensor status update
//    4    Sensor #  Byte  1..52 (Note that as of Sept 2017, our code ignores sensor 53 and we have disconnected it from the layout)
//    5    Trip/Clr  Byte  [0|1] 0-Cleared, 1=Tripped
//    6	   Checksum	 Byte  0..255

// A-MAS to A-SWT:  Command to set all turnouts to Last-known position
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 9
//    1	   To	       Byte	 5 (A_SWT)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  'L' = Last-known position
//    4..7 Parameter Byte  32 bits: 0=Normal, 1=Reverse
//    8	   Checksum  Byte  0..255

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
// Rev: 09/27/17
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

// A-MAS to A-LEG: Tell A-LEG if operator wants SMOKE, based on a/n query by A-OCC.  Registration mode only.
// Rev: 09/27/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  6
//   	1	  To	      Byte	2 (A-LEG)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'S' = Smoke for locos, yes or no
//    4   Reply     Char  [Y|N]
//    5   Cksum     Byte  0..255

// A-MAS to A-LEG: Tell A-LEG if operator wants FAST or SLOW loco startup, based on a/n query by A-OCC.  Registration mode only.
// Rev: 09/27/17
// OFFSET DESC      SIZE  CONTENTS
//    0   Length    Byte  6
//   	1	  To	      Byte	2 (A-LEG)
//   	2	  From	    Byte	1 (A_MAS)
//    3   Msg Type  Char  'F' = Does operator want fast or slow loco startup at registration?
//    4   Reply     Char  [F|S]
//    5   Cksum     Byte  0..255

// **************************************************************************************************************************

// We will start in MODE_UNDEFINED, STATE_STOPPED.  User must press illuminated Start button to start a mode.
byte modeCurrent  = MODE_UNDEFINED;
byte stateCurrent = STATE_STOPPED;

// *** SERIAL LCD DISPLAY CLASS:
#include <Display_2004.h>                // Class in quotes = in the A_SWT directory; angle brackets = in the library directory.
// Instantiate a Display_2004 object called "LCD2004".
// Pass address of serial port to use (0..3) such as &Serial1, and baud rate such as 9600 or 115200.
Display_2004 LCD2004(&Serial1, 115200);  // Instantiate 2004 LCD display "LCD2004."
Display_2004 * ptrLCD2004;               // Pointer will be passed to any other classes that need to be able to write to the LCD display.
char lcdString[LCD_WIDTH + 1];           // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** MESSAGE CLASS (Inter-Arduino communications):
#include <Message_MAS.h>
Message_MAS Message(ptrLCD2004);         // Instantiate message object "Message"; requires a pointer to the 2004 LCD display
byte MsgIncoming[RS485_MAX_LEN];         // Global array for incoming inter-Arduino messages.  No need to init contents.  Probably shouldn't call them "RS485" though.
byte MsgOutgoing[RS485_MAX_LEN];         // No need to initialize contents.

char occPrompt[9] = "        ";       // Stores a/n prompts sent to A-OCC, define here to avoid cross initialization error in switch stmt.

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

// firstTimeThrough is only true the first time into a Manual, Register, Auto, or Park section of do-the-mode code.
// We use it to determine if we should read from the FRAM1 control block to get the last-known-state of all trains,
// to use as a default when registering.
// We also use it to set all spur turnouts to mainline alignment whenever a new mode is started, even Manual.
// Not sure yet what else we might use this for...???
bool firstTimeThrough     = true;
bool smokeOn              = false;  // Defined during registration; turn on loco smoke when starting up?
bool slowStartup          = false;  // Defined during registration; fast or slow startup sequence when starting locos?
bool announcementsOn      = false;  // Defined during registration; make station arrival/departure announcements via PA?
bool registrationComplete = false;  // registrationComplete is used during Registration mode...

// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// *** ROUTE REFERENCE TABLES...
// Note 9/12/16 added maxTrainLen which is typically the destination siding length

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

// BLOCK RESERVATION TABLE...
// Rev: 09/06/17.
// Used only in A-MAS and A-LEG as of 01/21/17; possibly also A-OCC?
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
// 09/25/17: WHICH BLOCK AND WHICH END IS A GIVEN SENSOR NUMBER.  Offset by 1 (0..51 for 52 sensors).
// There will be 52 records for sensors 1..52, implied by array offset (0..51) + 1
struct sensorBlockStruct {
  byte whichBlock;    // 1..26
  char whichEnd;      // E or W
};
sensorBlockStruct sensorBlock[TOTAL_SENSORS] = { 1,'W',1,'E',2,'W',2,'E',3,'W',3,'E',4,'W',4,'E',5,'W',5,'E',6,'W',6,'E',14,'W',14,'E',15,'W',15,'E',13,'W',13,'E',19,'W',19,'E',20,'W',20,'E',23,'W',23,'E',24,'W',24,'E',25,'W',25,'E',26,'W',26,'E', 7,'E', 7,'W', 8,'E', 8,'W', 9,'E', 9,'W',10,'E',10,'W',11,'E',11,'W',12,'W',12,'E',16,'W',16,'E',17,'W',17,'E',18,'W',18,'E',22,'E',22,'W',21,'W',21,'E' };

// Create a 64-byte array to hold the status of every sensor 1..64 (sensor 1 is at element 0.)  0 = clear, 1 = occupied.
byte sensorStatus[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
// *** SENSOR STATUS UPDATE TABLE: Store the sensor number and change type for an individual sensor change, to pass to functions etc.
struct sensorUpdateStruct {
  byte sensorNum;     // 0 = Real sensor number i.e. 1 to 52 (no such thing as sensor zero.)
  byte changeType;  // 0 = Cleared, 1 = Tripped
};
sensorUpdateStruct sensorUpdate = {0, 0};

// We're going to define all structures that hold static data so that we have an easy place to find it if it
// needs to be updated in the future.  Each of these structures has a DATE so we can keep track.
// ALL SOURCE FILES will need to be updated when any of this data changes.

// TRAIN REFERENCE TABLE Rev: 01-15-17.
// BE SURE TO ALSO UPDATE IN OTHER MODULES THAT USE THIS STRUCTURE OR HARD-CODED DATA IF CHANGED.
// Used only in A-MAS and A-LEG as of 01/21/17.
// VERSION 01/15/17 added short and long whistle/horn parameters needed for Delayed Action table
// VERSION 09/24/16 added train length to train ref table.  Prev 07/15/16

struct trainReferenceStruct {
  char alphaDesc[9];             // Alphanumberic description only used by A-OCC when prompting operator for train IDs during registration
  char engOrTrain;               // E or T for Legacy commands (we are using Legacy "trains" exclusively)
  byte legacyID;                 // 2-digit Legacy/TMCC engine or train number.  Note: 91, 92, 93, 94 reserved for PowerMasters.
  char steamOrDiesel;            // S or D for Steam or Diesel
  char freightOrPassenger;       // F or P
  byte trainLen;                 // In inches
  unsigned int shortTootLen;     // How long to hold short toot before ending with quilling horn intensity zero.
  unsigned int shortTootDelay;   // How long to wait after stopping toot before starting another toot.
  byte shortTootIntensity;       // Quilling horn intensity 1..15 (likely close to 1 for short)
  unsigned int longTootLen;      // How long to hold each section of a short toot (need to combine several)
  byte longTootRepeats;          // How many times to repeat the long toot time to make a single long toot
  unsigned int longTootDelay;    // How many ms to wait after the end of a long toot before starting another toot
  byte longTootIntensity;        // Quilling horn intensity 1..15 (likely close to 15 for long)
  byte crawlSpeed;               // 0..200 Legacy speed setting to use for this speed
  unsigned int crawlRate;        // mm/sec at above speed setting
  byte crawlRPM;                 // RPMs for Legacy to set at this speed
  byte lowSpeed;                 // 0..200 Legacy speed setting to use for this speed
  unsigned int lowRate;          // mm/sec at above speed setting
  byte lowRPM;                   // RPMs for Legacy to set at this speed
  byte mediumSpeed;              // 0..200 Legacy speed setting to use for this speed
  unsigned int mediumRate;       // mm/sec at above speed setting
  byte mediumRPM;                // RPMs for Legacy to set at this speed
  byte highSpeed;                // 0..200 Legacy speed setting to use for this speed
  unsigned int highRate;         // mm/sec at above speed setting
  byte highRPM;                  // RPMs for Legacy to set at this speed
};

// TRAIN REFERENCE DATA Rev: 10/01/17.
// This is a READ-ONLY table, total size around 328 bytes.  Could be stored in FRAM 3.
// Note that train number 1..8 corresponds with element 0..7.
// This is also our CROSS REFERENCE where we lookup the Legacy ID (and if engine or train), versus the "1 thru 8" programming train number.
// IMPORTANT: Easier just to hard-code here vs. moving into FRAM because easier to make changes.
// HOWEVER, this is used by both A-LEG *and* A-MAS so be sure to check "version number" and change in both as needed.
trainReferenceStruct trainReference[MAX_TRAINS] = {
  {"BIG BOY ",'T',  1, 'S', 'F',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"WP 803  ",'T',  2, 'D', 'F',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"SF 1484 ",'T',  3, 'S', 'P',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"SF PA 52",'T',  4, 'D', 'F',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"SP RF-16",'T',  5, 'D', 'P',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"SF 3751 ",'T',  6, 'S', 'P',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"MOHAWK  ",'T',  7, 'S', 'F',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,},
  {"WORK TRN",'T',  8, 'D', 'F',  72, 100, 350, 1, 250, 4, 1000, 15, 20, 90, 1, 50, 135, 3, 70, 200, 5, 100, 350, 7,}
};

// TURNOUT RESERVATION TABLE...
// Each element holds the TRAIN NUMBER 1..8 that the turnout is reserved for.  0 = Not reserved.
struct turnoutReservationStruct {
  byte turnout; 
};
turnoutReservationStruct turnoutReservation[TOTAL_TURNOUTS] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

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

// LAST KNOWN TURNOUT POSITION TABLE.
// Oonly four bytes = 32 bits.  0 = Normal, 1 = Reverse.
byte lastKnownTurnout[4];

// TRAIN LAST KNOWN LOCATION TABLE to hold train number, block number, and direction for the last known 
// position of each train (i.e. after a controlled end of Register, Auto, or Park mode.)
// Data is read from and saved to the control register of FRAM1 (following the 3-byte revision date, so starting at address 3.)
// trainNum is really redundant here, could get rid of it: train number = element offset (0..7) plus 1 (1..8)
struct lastLocationStruct {
  byte trainNum;    // 1..8
  byte blockNum;    // 1..26
  char whichDirection;  // E|W
};
lastLocationStruct lastTrainLoc[MAX_TRAINS] = {    // Defaults for each of the 8 trains
  { 1, 0, ' ' },
  { 2, 0, ' ' },
  { 3, 0, ' ' },
  { 4, 0, ' ' },
  { 5, 0, ' ' },
  { 6, 0, ' ' },
  { 7, 0, ' ' },
  { 8, 0, ' ' }
};

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485  up to 115200
  initializePinIO();                    // Initialize all of the I/O pins
  LCD2004.init();                       // Initialize the 20 x 04 Digole serial LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  initializeFRAM1AndGetControlBlock();  // Check FRAM1 version, and get control block
  sprintf(lcdString, "FRAM1 Rev. %02i/%02i/%02i", FRAM1GotVersion[0], FRAM1GotVersion[1], FRAM1GotVersion[2]);  // FRAM1 version no. on LCD display
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  //  initializeFRAM2();

  // We need some delay in order to give the slaves a chance to get ready to receive data.
  delay(1000);

  // Send commands to A-SWT to set every turnout to the last-known state -- so we can be sure of the actual state of every
  // turnout, without changing the existing settings any more than necessary.
  // These commands will also be seen by A-LED so it can set the green control panel LEDs to match actual turnout orientation.
  RS485fromMAStoSWT_SetLastKnown();

  // 1/21/17: Shall we get the status of all non-occupied sensors right off the bat?
  // We have a two-byte structure to hold a received updated sensor status: sensorUpdate.sensorNum, sensorUpdate.changeType.
  // And we have a 64-byte array to hold the status of every sensor 1..64 (sensor 1 is at element 0.)  0 = clear, 1 = occupied.
  // byte sensorStatus[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  // However, note that our loop checks, and if nothing else is going on (i.e. when starting and no mode), A-MAS will allow A-SNS
  // to dump all of the occupied sensors pretty quickly.

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  // First time into loop() we are in MODE_UNDEFINED, STATE_STOPPED, firstTimeThrough = true.  Mode, Start, and Stop LEDs are all off.
  // modeCurrent can be MODE_UNDEFINED, MODE_MANUAL, MODE_REGISTER, MODE_AUTO, MODE_PARK, or MODE_POV
  // stateCurrent can be STATE_UNDEFINED, STATE_RUNNING, STATE_STOPPING, or STATE_STOPPED
  // firstTimeThrough can be true or false.
  // WAIT - PER ABOVE, is firstTimeThrough = true or can it be true or false?  Fix this comment when I figure it out.

  // *********************************************************************************************************************************
  // ************************************* CHECK TO SEE IF OPERATOR WANTS TO CHANGE MODE OR STATE ************************************
  // *********************************************************************************************************************************

  // If we are stopped, then we need to monitor and respond to changes in the selector switch and START button.
  if (stateCurrent == STATE_STOPPED) {
    checkIfRequestToStartNewMode();
  }

  // At this point, stateCurrent could still be STOPPED, or we may have set it to RUNNING with a new modeCurrent.
  // Or stateCurrent could be RUNNING or STOPPING and been that way for a while.
  // firstTimeThrough will only be true if we *just* set stateCurrent to RUNNING and assigned a new modeCurrent, after previous state was STOPPED.

  // If we are running, then we need to monitor and respond to changes in the selector switch STOP button.
  // In any mode, the following will change state to STOPPING if operator pressed illuminated stop button, and turn off stop button illumination.
  if (stateCurrent == STATE_RUNNING) {
    checkIfRequestToStopMode();
  }

  // Now we are in whatever mode and state the operatator wants to be in, so execute that mode and state.
  // If stateCurrent is RUNNING or STOPPING, go ahead and run the RUNNING loop.
  // Here is how STOPPING differs from RUNNING in the various modes:
  //   MANUAL and POV: STOPPING mode transitions immediately to STOPPED mode.  There is nothing to do.
  //   REGISTER: Normally A-MAS will know when registration is complete and will change the mode from RUNNING to STOPPED when A-OCC sends
  //     the data for train locations etc.  If the operator presses STOP, then there is a problem and we will trigger Halt and quit.
  //     The operator must start over in Manual or Register mode.
  //   AUTO: When STOPPING, don't schedule any new trains, but follow Train Progress and maintain STOPPING mode until all trains have tripped their
  //     Destination Exit sensors.  When that happens, all trains will have physically stopped, and we can enter STOPPED mode.
  //   PARK: Same as AUTO mode STOPPING.  Continue until all running trains have reached their destination.  Note that in Park mode, we
  //     park trains one at a time, so it's possible that all trains are not "parked".  But we don't care, no problem.

  // So the following block of primary "do the mode" code will change the mode from STOPPING to STOPPED when appropriate.
  // When we are stopping, and stopped, and periodically when running, write the last-known-train-position/direction to the FRAM1 control buffer.
  // Also we will check for the firstTimeThrough flag which will trigger code to read the last-known-train-position table etc.

  // *********************************************************************************************************************************
  // ************************************* RUN THE MAIN 'RUNNING' LOGIC FOR EACH OF THE FIVE MODES ***********************************
  // *********************************************************************************************************************************

  if (stateCurrent == STATE_RUNNING || stateCurrent == STATE_STOPPING) {    // Here we go with the main "get 'er done" code!

    if (firstTimeThrough) {    
      // We will also have a "first time through" block in each of the five mode blocks, and that is where we will also clear the "first-time" flag.
      // FUTURE ENHANCEMENT: Whenever we start a new mode, as a safety precaution, align all spur turnouts to the mainline.
      // Be sure to update the turnout status array and save to FRAM1 control block.
      // There are only a few of them on the current layout, and we only do it here...so maybe just hard-code, or use a small array.
      // T09R, T11R, T13R, T22R, T19N, T23N, T24N, T2NR, T26R
    }

    // Since we're RUNNING or STOPPING (in any mode,) we always want the latest sensor-change updates, so check now.
    // We want to get all of them in case we're just starting a mode such as AUTO and may not get back to this part of the loop right away...
    // No need to broadcast the change, as other Arduinos that care will see the OCC messages on the RS485 bus and act accordingly.
    while (sensorChanged(&sensorUpdate.sensorNum, &sensorUpdate.changeType)) {
      if (sensorUpdate.changeType == 0) {   // 0 = cleared
        sprintf(lcdString,"Sensor %2d cleared.", sensorUpdate.sensorNum);
      } else {                          // 1 = tripped
        sprintf(lcdString,"Sensor %2d tripped.", sensorUpdate.sensorNum);
      }
      LCD2004.send(lcdString);
      // Update our 64-element array that simply tracks 0=clear or 1=occupied for all sensors.
      sensorStatus[sensorUpdate.sensorNum - 1] = sensorUpdate.changeType;  // Remember, sensor #1 status is at sensorStatus[0]
      for (byte i=0; i < sizeof(sensorStatus); i++) {   // 0..63 for sensors 1..64
        Serial.print(sensorStatus[i]);
      }
      Serial.println();
    }
    // Now, for any mode that is RUNNING or STOPPING, the sensorStatus[] array is fully up to date with the latest changes.

    switch (modeCurrent) {

      // *********************************************************************************************************************************
      // ********************************************** MAIN LOGIC FOR 'MANUAL' AND 'POV' MODE *******************************************
      // *********************************************************************************************************************************

      case MODE_MANUAL:
      case MODE_POV:      // POV same as manual mode, for now...except which control panel mode light to turn off.
        // Okay, so we are in MANUAL or POV mode, and the current state is either RUNNING or STOPPING.
        // Just watch for control panel turnout button presses, and update the various block, sensor, and turnout LEDs on panel.
        throwTurnoutIfRequested();  // Check to see if A-BTN wants to send us a button press, and if so, get it and send command to A-SWT to throw it etc.
        if (stateCurrent == STATE_STOPPING) {
          // FUTURE: We might look at occupancy sensors to be sure nobody is fouling the mainline, but for now trust operator to not do that.
          stateCurrent = STATE_STOPPED;
          sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
          digitalWrite(PIN_ROTARY_LED_MANUAL, HIGH);   // Turn off "manual mode" LED by setting output HIGH.  on = LOW.
          digitalWrite(PIN_ROTARY_LED_POV, HIGH);   // Turn off "POV mode" LED by setting output HIGH.  on = LOW.
        }
        firstTimeThrough = false;     // Nothing more special to be done, so clear the "first-time" flag
        break;  // End of MANUAL and POV mode block

      // *********************************************************************************************************************************
      // ************************************************* MAIN LOGIC FOR 'REGISTER' MODE ************************************************
      // *********************************************************************************************************************************

      case MODE_REGISTER:
        // 01-22-17 Outline:
        // Registration code is "blocking" in that nothing else happens until Registration is complete.
        // firstTimeThrough is moot here because we only enter this block one time, when we first start Register mode.
        // Be sure to check for the Halt pin, since we stay in this loop and don't exit to the main loop() until registration ends!
        // Of course, if anybody else trips Halt, then everyone else including A-LEG will stop, so even if A-MAS keeps sending out
        // commands to A-LEG, A-SWT, etc., they will be disregarded and no harm done.
        // When we enter this block of code, we will already have all known sensor status from A-SNS.
        // Use global 8-byte (plus newline) char array "occPrompt" to set 8-char a/n prompts to be sent to A-OCC via RS485.
        // We won't monitor the STOP button during Registration; the operator will need to Halt or Reset if they want to bail.

        initializeTurnoutReservationTable();   // INITIALIZE THE TURNOUT RESERVATION TABLE to clear all entries as unreserved.
        initializeBlockReservationTable();     // INITIALIZE THE BLOCK RESERVATION TABLE to set all blocks as unreserved.
        for (byte i = 0; i < MAX_TRAINS; i++) {
          trainProgressInit(i + 1);  // First train is Train #1 (not zero) because functions accept actual train numbers, not zero-offset numbers.
        }

        // Have A-OCC prompt operator if they want smoke turned on or off when new trains are registered and started up.
        smokeOn = RS485AskOCCtoPromptForSmoke();
        // Pass the response to A-LEG.
        RS485TellLEGifUseSmoke(smokeOn);

        // Now have A-OCC prompt if operator wants Fast or Slow loco startup as they are registered.
        slowStartup = RS485AskOCCtoPromptStartupSpeed();
        // Pass the response to A-LEG.
        RS485TellLEGStartupSpeed(slowStartup);

        // Now have A-OCC a prompt if operator wants to hear PA system audio from A-OCC.
        // Unlike smoke and starup speed, we will not be forwarding this to A-LEG; it's for A-MAS use only.
        announcementsOn = RS485AskOCCtoPromptAnnouncements();

        // Okay!  Time to register trains!
        // Start by populating the Block Reservation table to reserve all occupied blocks for the STATIC train #TRAIN_STATIC.
        // As "real" trains are registered, we will update those block reservations accordingly.
        // Note that all block and turnout reservations have just been initialized, so nothing is reserved yet.
        // We have also initialized the Train Progress table so that no trains are active anywhere yet.

        // For each sensor, if it is tripped
        //   Determine the block number
        //   Update BlockReservationTable for that block for train #TRAIN_STATIC (default all occupied blocks to STATIC.)
        // For each possible train (currently max of 8) send number (1..8) and name (8-char) to A-OCC, along with a default block number if it can be
        //   found in the last-known-block table, and tell it to query operator to register them.
        //   Send all records in one go, so A-OCC will be able to present all choices to operator for each occupation query.
        //   A-OCC will add two prompts in addition to the max 8 train names: STATIC and DONE.  When operator selects DONE it means that all real
        //     trains have been registered, and the rest (if any) should be considered static.
        // As registration entries come back, update BlockReservationTable for that block with the identified (never static) train, and tell A-LEG
        //   to create a new train and run the startup sequence, etc.  Also update last-known-block for this train and write FRAM1 control
        //   block.
        // A-OCC does not need to bother sending back a list of static train block numbers, because A-MAS must assume that all unidentified blocks
        //   must be static trains.  
        // If A-MAS and A-OCC ever don't agree on exactly which blocks are occupied and which are free, we won't know it, but it's a bug.
        // Since there isn't a way for A-OCC to see operator press Stop during registration, A-MAS willignore it -- operator must Reset or Halt to
        //   terminate registration, unless they choose the DONE option when selecting train descriptions in A-OCC registration.

        // Given an occupied sensor, figure out which block, and which end of that block, the sensor is on.
        // The train is always assumed to be facing the direction of the end that the sensor occupies.  So if a sensor is at the
        // East end of Block 3, then a train is assumed to be in block 3 facing East.
        // We just intitialized block reservations so that ALL blocks are unreserved (Train = 0.)
        // Initially we'll just reserve all occupied blocks for static train = TRAIN_STATIC.  Then we'll ask A-OCC to have the operator
        // tell us which of those are occupied by "real" trains, and which specific trains...then we will update the Block
        // Reservation table again -- at which time our trains will be "registered."
        for (byte sensor=0; sensor < TOTAL_SENSORS; sensor++) {   // 0..63 for sensors 1..64
          if (sensorStatus[sensor] == 1) {  // If this sensor is occupied, determine the block and direction train must be facing
            blockReservation[sensorBlock[sensor].whichBlock - 1].reservedForTrain = TRAIN_ID_STATIC;  // Default occupancy to "static train"
            blockReservation[sensorBlock[sensor].whichBlock - 1].whichDirection = sensorBlock[sensor].whichEnd;   //  'E' or 'W'
          }
        }

        // TEST CODE: PRINT THE BLOCK RESERVATION TABLE SHOWING ALL OCCUPIED BLOCKS AS STATIC RESERVED...
        Serial.println("BEGINNING BLOCK RESERVATION TABLE, WITH ALL OCCUPIED BLOCKS RESERVED AS #99 STATIC:");
        Serial.println("Block, Reserved-For-Train, Direction:");
        for (byte i=0; i < TOTAL_BLOCKS; i++) {  // Block number is a zero offset, so record 0 = block 1
          Serial.print(i + 1); Serial.print("      ");
          Serial.print(blockReservation[i].reservedForTrain);  // Should be 0 or TRAIN_STATIC at this point
          Serial.print(i + 1); Serial.print("      ");
          Serial.println(blockReservation[i].whichDirection);         // Should be E or W
        }
        
        // *** SEND A-OCC A LIST OF ALL TRAINS AND LAST-KNOWN LOCATIONS, TO BE REGISTERED BY OPERATOR...
        // Now, every occupied block is reserved for train 99 (static) and all vacant blocks show train 0.
        // Start assembling and sending train-data records to A-OCC, which it will use when registering trains.
        // Fill the RS485 outgoing buffer as appropriate...
        Serial.println("Sending RS485 to A-OCC, and trying to find matching 'last-knonw-train-loc' records...");
        MsgOutgoing[RS485_LEN_OFFSET] = 16;   // Byte 0 is always the same length for this set of outgoing records
        MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_OCC;  // Byte 1 is always A-OCC.
        MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is always A-MAS.
        MsgOutgoing[3] = 'R';   // Byte 3 = Registration record.
        for (byte i = 0; i < MAX_TRAINS; i++) {   // Using a byte temp variable so one byte in MsgOutgoing[]
          MsgOutgoing[4] = (i + 1);  // Train number 1..8
          memcpy(MsgOutgoing + 5, trainReference[i].alphaDesc, 8);  // Drop the 8-byte a/n description field into the RS485 outgoing message field
          Serial.print("Sending alpha desc: "); Serial.println(trainReference[i].alphaDesc);
          // Insert the train's last-known block (from control record.)  This may not even be a currently-occupied block, but we'll let A-OCC deal with it.
          MsgOutgoing[13] = lastTrainLoc[i].blockNum;   // This should be the last-known block number, if any.  Else zero.
          Serial.print("Last-known block number: "); Serial.println(lastTrainLoc[i].blockNum);
          if (i < (MAX_TRAINS - 1)) {
            MsgOutgoing[14] = 'N';   // Not last record being sent
          } else {
            MsgOutgoing[14] = 'Y';   // Last record (last train)
          }
          MsgOutgoing[15] = calcChecksumCRC8(MsgOutgoing, 15); 
          RS485SendMessage(MsgOutgoing);
          delay(50);  // Just to be sure we don't overflow the A-OCC incoming serial buffer before data can be read
        }

        // *** STAND BY WHILE A-OCC SLOWLY RETURNS RECORDS FOR EACH TRAIN THE OPERATOR WISHES TO REGISTER...
        // Okay, we have sent a record for every train indicating the train's description and last-known block for A-OCC to use as a default when
        // registering trains with the operator.  Now wait for records to slowly come back from A-OCC, until all trains have been registered, or
        // the operator has indicated they are done.
        // Remember, the Block Reservation table is still initialized to all zeroes and TRAIN_STATIC for occupied blocks, so we will update the Block
        // Reservation record (which should be TRAIN_STATIC) for each train as it comes in.  Trains that are not returned by A-OCC here are assumed to
        // be either not on the layout, or in static blocks (marked as train TRAIN_STATIC.)  It's the same either way; we ignore them.  We will only be
        // getting actual train numbers that have been assigned to blocks by the operator in A-OCC, in these incoming RS485 messages...
        registrationComplete = false;
        while (!registrationComplete) {   // Do this loop until A-OCC tells us "all done registering trains" via an RS485 message)
          // Note that when the operator clicks "DONE" that all trains have been registered (or when A-OCC recognizes it because they have indicated a
          // block number for every possible train), A-OCC will send a record with *only* the "last record" field set to Y; i.e. no train data.
          while (RS485GetMessage(MsgIncoming) == false) {
            checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, just stop
          }   // Just sit here and loop while we wait for the next RS485 incoming reply
          if (RS485fromOCCtoMAS_RegisteredTrain()) {  // If A-OCC is sending train registration information selected by the operator.
            // Anything other than an incoming "registered train" record would be a bug, but so far we aren't checking for that.
            if (RS485fromOCCtoMAS_TrainRecord()) {    // If we have a record with a new registered train (versus empty "all done" record)
              // PROCESS INCOMING DATA FOR A SINGLE NEWLY-REGISTERED TRAIN...
              // Great!  We have a registered train record.  Get the data we need from the incoming RS485 buffer...
              // Note that A-OCC will not send us data for STATIC trains, only up to the 8 "real" trains.
              byte tTrain = TRAIN_ID_NULL;
              byte tBlock = 0;
              char tDir = ' ';
              byte tEntSns = 0;
              byte tExtSns = 0;
              RS485fromOCCtoMAS_ExtractData(&tTrain, &tBlock, &tDir, &tEntSns, &tExtSns);
              // Now, populate last-known train position/direction for this newly-registerd train, and update the FRAM1 Control Block...
              FRAM1UpdateLastKnown(tTrain, tBlock, tDir);
              // Now, update the block reservation table - this will change a "TRAIN_STATIC" block to the train number of this newly-registered train.
              blockReservationUpdate(tTrain, tBlock, tDir);
              // Now, create an entry in the newly-initialized Train Progress table for this newly-registered train...
              trainProgressEnqueue(tTrain, tBlock, tEntSns, tExtSns);
              // TEST CODE: DISPLAY RESULTS OF WHAT WE HAVE SO FAR...AS EACH TRAIN IS REGISTERED...
              Serial.println("NEW TRAIN REGISTERED!");
              Serial.print("Train number: "); Serial.println(tTrain);
              Serial.print("Block number: "); Serial.println(tBlock);
              Serial.print("Direction   : "); Serial.println(tDir);
              Serial.print("Entry sensor: "); Serial.println(tEntSns);
              Serial.print("Exit sensor : "); Serial.println(tExtSns);
              // 9/20/17: A-LEG will have seen the above commands come in, and will just now have registered and be starting up each train.
              // A-LEG will see that there is no route, and we are not in Auto mode, so it won't try to get the train moving yet.
              // But A-LEG will create a new Train Progress record for this train, initialized to just this single block.
              // One reason for having A-LEG snoop the RS485 bus for A-OCC registered-train commands is because A-MAS has given
              // A-OCC permission to transmit train registration data until all trains have been registered.  If A-MAS takes time
              // to send an RS485 command to A-LEG when it is waiting for the next registration message from A-OCC, we could have
              // RS485 messages stepping on each other, and the additional messages might confuse A-OCC, which isn't expecting to
              // see incoming traffic on the RS485 data bus.
            } else {   // Rather than being a train data record, we received an "all done" record from A-OCC
              registrationComplete = true;
              Serial.println("Registration complete.");
            }
          }
        }  // End of "while registration is not complete" code block

        // Okay, now we have finished receiving all of the train registration data from A-OCC, and updated the last-known-train-position,
        // block reservation, and train progress tables, and A-LEG will have created a new Train Progress record for each train, and start
        // up each train.
        // Now we should have a fresh TRAIN PROGRESS table, and ready-to-go BLOCK RESERVATION and TURNOUT RESERVATION tables.  All done!
        // There is nothing special to be done when "stopping" in Register mode
        stateCurrent = STATE_STOPPED;
        digitalWrite(PIN_ROTARY_LED_STOP, HIGH);       // Turn off the Stop button LED since we stopped automatically in this case
        digitalWrite(PIN_ROTARY_LED_REGISTER, HIGH);   // Turn off "register mode" LED by setting output HIGH.  on = LOW.
        firstTimeThrough = false;     // Nothing more special to be done, so clear the "first-time" flag
        sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
        break;  // End of REGISTER mode block

      // *******************************************************************************************************************************
      // *************************************************** MAIN LOGIC FOR 'AUTO' MODE ************************************************
      // *******************************************************************************************************************************

      case MODE_AUTO:

        if (stateCurrent == STATE_STOPPING) {    // Here is where we put code to accomplish the stop.  
          delay(3000);  // until we get some real code to do something
          stateCurrent = STATE_STOPPED;  // For now, just pretend we've done what needs doing to bring to a controlled stop
          sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
        }
        if (stateCurrent == STATE_STOPPED) {    // If we've stopped, turn off the mode LED
          digitalWrite(PIN_ROTARY_LED_AUTO, HIGH);   // Turn off "auto mode" LED by setting output HIGH.  on = LOW.
        }
        firstTimeThrough = false;     // Nothing more special to be done, so clear the "first-time" flag
        break;  // End of AUTO mode block

      // *******************************************************************************************************************************
      // *************************************************** MAIN LOGIC FOR 'PARK' MODE ************************************************
      // *******************************************************************************************************************************

      case MODE_PARK:

        if (stateCurrent == STATE_STOPPING) {    // Here is where we put code to accomplish the stop.  
          delay(3000);  // until we get some real code to do something
          stateCurrent = STATE_STOPPED;  // For now, just pretend we've done what needs doing to bring to a controlled stop
          sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
        }
        if (stateCurrent == STATE_STOPPED) {    // If we've stopped, turn off the mode LED
          digitalWrite(PIN_ROTARY_LED_PARK, HIGH);   // Turn off "park mode" LED by setting output HIGH.  on = LOW.
        }
        firstTimeThrough = false;     // Nothing more special to be done, so clear the "first-time" flag
        break;  // End of PARK mode block

    }   // end of switch/case block, which handles each mode when stateCurrent is either RUNNING or STOPPING
  }   // end of "if current mode is RUNNING or STOPPING" main functional code
}  // end of main loop()

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
  digitalWrite(PIN_FRAM1, HIGH);    // Chip Select (CS): pull low to enable
  pinMode(PIN_FRAM1, OUTPUT);
  digitalWrite(PIN_FRAM2, HIGH);    // Chip Select (CS): pull low to enable
  pinMode(PIN_FRAM2, OUTPUT);
  pinMode(PIN_REQ_TX_A_LEG_IN, INPUT_PULLUP);   // A-LEG will pull this pin LOW when it wants to send A-MAS a message
  pinMode(PIN_REQ_TX_A_SNS_IN, INPUT_PULLUP);   // Ditto for A-SNS i.e. an occupancy sensor has been tripped or cleared
  pinMode(PIN_REQ_TX_A_BTN_IN, INPUT_PULLUP);   // Ditto for A-BTN i.e. a button has been pressed
  digitalWrite(PIN_HALT, HIGH);
  pinMode(PIN_HALT, INPUT);                  // HALT pin: monitor if gets pulled LOW it means someone tripped HALT.  Or change to output mode and pull LOW if we want to trip HALT.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
  pinMode(PIN_ROTARY_MANUAL, INPUT_PULLUP);  // Pulled LOW when selected on control panel.  Same with following...
  pinMode(PIN_ROTARY_REGISTER, INPUT_PULLUP);
  pinMode(PIN_ROTARY_AUTO, INPUT_PULLUP);
  pinMode(PIN_ROTARY_PARK, INPUT_PULLUP);
  pinMode(PIN_ROTARY_POV, INPUT_PULLUP);
  digitalWrite(PIN_ROTARY_LED_MANUAL, HIGH);
  pinMode(PIN_ROTARY_LED_MANUAL, OUTPUT);
  digitalWrite(PIN_ROTARY_LED_REGISTER, HIGH);
  pinMode(PIN_ROTARY_LED_REGISTER, OUTPUT);
  digitalWrite(PIN_ROTARY_LED_AUTO, HIGH);
  pinMode(PIN_ROTARY_LED_AUTO, OUTPUT);
  digitalWrite(PIN_ROTARY_LED_PARK, HIGH);
  pinMode(PIN_ROTARY_LED_PARK, OUTPUT);
  digitalWrite(PIN_ROTARY_LED_POV, HIGH);
  pinMode(PIN_ROTARY_LED_POV, OUTPUT);
  pinMode(PIN_ROTARY_START, INPUT_PULLUP);
  pinMode(PIN_ROTARY_STOP, INPUT_PULLUP);
  digitalWrite(PIN_ROTARY_LED_START, HIGH);
  pinMode(PIN_ROTARY_LED_START, OUTPUT);
  digitalWrite(PIN_ROTARY_LED_STOP, HIGH);
  pinMode(PIN_ROTARY_LED_STOP, OUTPUT);
  return;
}

void initializeTurnoutReservationTable() {
  for (int i = 0; i < TOTAL_TURNOUTS; i++) {            // For each turnout
    turnoutReservation[i].turnout = 0;
  }
  return;
}

void initializeBlockReservationTable() {
  // Re-initialize block reservations whenever we begin a new Auto mode.
  for (byte b = 1; b <= TOTAL_BLOCKS; b++) {
    byte t = TRAIN_ID_NULL;
    char d = ' ';
    blockReservationUpdate(t, b, d);
  }
  return;
}

void blockReservationUpdate(const byte t, const byte b, const char d) {
  // This is used for both reserving a block (given train number > 0) or for releasing a reserved block (setting train number to zero.)
  blockReservation[b - 1].reservedForTrain = t;
  blockReservation[b - 1].whichDirection = d;
  Serial.print("Setting block reservation for block: "); Serial.print(b); Serial.print(" train "); Serial.print(t); Serial.print(" direction "); Serial.println(d);
  return;
}

void sendRS485ModeBroadcast(byte tmode, byte tstate) {
  // 10/29/16: Tell everyone via RS485 about our new state.
  // Also display the new mode and state on the LCD and Serial monitor.
  if (tmode == 1) {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "MANUAL RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "MANUAL STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "MANUAL STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "MANUAL UNKNOWN");
          break;
      }
  } else if (tmode == 2) {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "REGISTER RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "REGISTER STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "REGISTER STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "REGISTER UNKNOWN");
          break;
      }
  } else if (tmode == 3) {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "AUTO RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "AUTO STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "AUTO STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "AUTO UNKNOWN");
          break;
      }
  } else if (tmode == 4) {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "PARK RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "PARK STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "PARK STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "PARK UNKNOWN");
          break;
      }
  } else if (tmode == 5) {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "P.O.V. RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "P.O.V. STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "P.O.V. STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "P.O.V. UNKNOWN");
          break;
      }
  } else {
      switch (tstate) {
        case 1:
          sprintf(lcdString, "%.20s", "UNDEFINED RUNNING");
          break;
        case 2:
          sprintf(lcdString, "%.20s", "UNDEFINED STOPPING");
          break;
        case 3:
          sprintf(lcdString, "%.20s", "UNDEFINED STOPPED");
          break;
        default:
          sprintf(lcdString, "%.20s", "UNDEFINED UNKNOWN");
          break;
      }
  }
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  MsgOutgoing[RS485_LEN_OFFSET] = 7;   // Message byte 0 is length
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_ALL;  // Message byte 1 is "to"
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Message byte 2 is "from"
  MsgOutgoing[3] = 'M';  // Message byte 3 is char 'M' for Mode message
  MsgOutgoing[4] = tmode;  // Message byte 4 is byte for modeCurrent
  MsgOutgoing[5] = tstate;  // Message byte 5 is byte for stateCurrent
  MsgOutgoing[6] = calcChecksumCRC8(MsgOutgoing, 6);   // Message byte 6 is CRC checksum
  RS485SendMessage(MsgOutgoing);
  return;
}

void throwTurnout(byte tnum, char tdir) {
  // 03/15/17: Throw a specific turnout a given way (normal or reverse.)  Only called when in Manual mode.
  // 78 = ASCII 'N' = normal
  // 82 = ASCII 'R' = reverse
  if (tdir == 'N') {
    sprintf(lcdString, "Throw %2d N", tnum);
  } else if (tdir == 'R') {
    sprintf(lcdString, "Throw %2d R", tnum);
  } else {
    sprintf(lcdString, "%.20s", "throwTurnout ERR!");
  LCD2004.send(lcdString);
  endWithFlashingLED(6);
  }
  LCD2004.send(lcdString);
  Serial.println(lcdString);  
  MsgOutgoing[RS485_LEN_OFFSET] = 6;   // Byte 0.  Length is 6 bytes: Length, To, From, N|R, Turnout_Num, CRC
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_SWT;  // Byte 1.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2.
  MsgOutgoing[3] = tdir;
  MsgOutgoing[4] = tnum;
  MsgOutgoing[5] = calcChecksumCRC8(MsgOutgoing, 5); 
  RS485SendMessage(MsgOutgoing);
  return;
}

void throwRoute(char ttype, byte tnum) {    // Pass it char 'T'=Route, '1'=Park 1, or '2'=Park 2
  // 1/21/17: Throw an entire route of turnouts, when we assign new routes in Auto or Park mode...
  // Confirm we have a legit request record format...
  if ((ttype != 'T') && (ttype != '1') && (ttype != '2')) {
      sprintf(lcdString, "Bad throwRoute!");
      LCD2004.send(lcdString);
      Serial.print(lcdString);
      endWithFlashingLED(6);
  }
  MsgOutgoing[RS485_LEN_OFFSET] = 6;   // Byte 0.  Length is 6 bytes: Length, To, From, T|1|2, Route_Num, CRC
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_SWT;  // Byte 1.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2.
  MsgOutgoing[3] = ttype;    // T or 1 or 2
  MsgOutgoing[4] = tnum;  // This is the route number
  MsgOutgoing[5] = calcChecksumCRC8(MsgOutgoing, 5); 
  RS485SendMessage(MsgOutgoing);
  return;
}

void throwTurnoutIfRequested() {
  // This is only called (periodically) when in MANUAL and maybe P.O.V. modes.
  // Checks to see if operator pressed a "throw turnout" button on the control panel, and executes if so.
  // Check status of pin 8 (coming from A-BTN) and see if it's been pulled LOW, indicating button was pressed.
  if ((digitalRead(PIN_REQ_TX_A_BTN_IN)) == LOW) {    // A-BTN wants to send us an RS485 message a turnout button has been pressed
    // Send RS485 message to A-BTN asking for turnout button press update.
    // A-BTN only knows of a button press; it doesn't know what state the turnout was in or which turnout LED is lit.
    MsgOutgoing[RS485_LEN_OFFSET] = 5;   // Byte 0.  Length is 5 bytes: Length, To, From, 'B', CRC
    MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_BTN;  // Byte 1.
    MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2.
    MsgOutgoing[3] = 'B';  // "Send button press info"
    MsgOutgoing[4] = calcChecksumCRC8(MsgOutgoing, 4); 
    RS485SendMessage(MsgOutgoing);
    // Now wait for a reply...
    // There is NO legitimate reason why we would get ANY RS485 message from anyone except A-BTN at this point.
    MsgIncoming[RS485_TO_OFFSET] = 0;   // Anything other than ARDUINO_BTN
    do {
      RS485GetMessage(MsgIncoming);  // Only returns with data if it has a complete new message
    } while (MsgIncoming[RS485_TO_OFFSET] != ARDUINO_MAS);
    // We should NEVER get a message that isn't to A-MAS from A-BTN, but we'll check anyway...
    if ((MsgIncoming[RS485_TO_OFFSET] !=  ARDUINO_MAS) || (MsgIncoming[RS485_FROM_OFFSET] != ARDUINO_BTN)) {
      sprintf(lcdString, "%.20s", "RS485 not from BTN!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    byte turnoutToThrow = MsgIncoming[4];   // Button number 1..32
    if ((turnoutToThrow < 1) || (turnoutToThrow > TOTAL_TURNOUTS)) {
      sprintf(lcdString, "%.20s", "RS485 bad button no!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // Okay, the operator pressed a control panel pushbutton to toggle a turnout, and we have the turnout number 1..32.
    // Need to figure out which way the turnout is currently set so we can throw it the other way, and update the
    // last-known turnout position variable...
    // Retrieve what we believe to be the current orientation for this turnout from last-known turnout data.
    // Then flip that bit, update the FRAM1 control block with updated last-known turnout data.
    // Then, send a message to A-SWT to throw the turnout as indicated.
    // Note that we count bits and bytes starting from left to right.
    byte bitToCheck = turnoutToThrow - 1;   // Result will be bit 0..31, versus turnout 1..32
    char directionToThrow = ' ';            // Will be set to either N or R (normal or reverse)
    if (bitToCheck < 8) {           // 0..7 = left-most byte of the four
      byte byteToWrite = 0;
      byte bitIsSet = bitRead(lastKnownTurnout[byteToWrite], bitToCheck);
      if (bitIsSet) {  // Set bit means it is currently Reverse
        directionToThrow = 'N';
        bitClear(lastKnownTurnout[byteToWrite], bitToCheck);
      } else {         // Cleared bit means it is currently Normal
        directionToThrow = 'R';
        bitSet(lastKnownTurnout[byteToWrite], bitToCheck);
      }
    } else if (bitToCheck < 16) {    // 8..15 = 2nd byte from left
      byte byteToWrite = 1;
      byte bitIsSet = bitRead(lastKnownTurnout[byteToWrite], bitToCheck - 8);
      if (bitIsSet) {
        directionToThrow = 'N';
        bitClear(lastKnownTurnout[byteToWrite], bitToCheck - 8);
      } else {
        directionToThrow = 'R';
        bitSet(lastKnownTurnout[byteToWrite], bitToCheck - 8);
      }
    } else if (bitToCheck < 24) {    // 16..23 3rd byte from left
      byte byteToWrite = 2;
      byte bitIsSet = bitRead(lastKnownTurnout[byteToWrite], bitToCheck - 16);
      if (bitIsSet) {
        directionToThrow = 'N';
        bitClear(lastKnownTurnout[byteToWrite], bitToCheck - 16);
      } else {
        directionToThrow = 'R';
        bitSet(lastKnownTurnout[byteToWrite], bitToCheck - 16);
      }
    } else {                        // 24..31 = far-right byte
      byte byteToWrite = 3;
      byte bitIsSet = bitRead(lastKnownTurnout[byteToWrite], bitToCheck - 24);
      if (bitIsSet) {
        directionToThrow = 'N';
        bitClear(lastKnownTurnout[byteToWrite], bitToCheck - 24);
      } else {
        directionToThrow = 'R';
        bitSet(lastKnownTurnout[byteToWrite], bitToCheck - 24);
      }
    }
    updateFRAM1ControlBlock();  // Write control block including updated last-known turnout orientations...
    // Now send RS485 message to A-SWT and tell it to throw the turnout
    // Note that A-LED will see this command and update the control panel turnout LED automatically.
    throwTurnout(turnoutToThrow, directionToThrow);
  }   // end of "if a button was pressed, then throw a turnout" block
  return;
}

bool sensorChanged(byte * tNum, byte * tStatus) {
  // 09/30/17: Return true if we have an updated sensor status, else false.  Also populate tNum and tStatus with the updated data.
  // Since the two parms are passed by value, be sure the call uses the & address operator.
  // Sample call: if (sensorChanged(&sensorUpdate.sensorNum, &sensorUpdate.status)) {}
  // Real sensor number 1..52 (no such sensor as zero, fyi), status 0=cleared or 1=tripped.
  // If A-SNS detects a sensor change on the layout, it will pull a digital pin low to ask us to query it.
  if ((digitalRead(PIN_REQ_TX_A_SNS_IN)) == LOW) {     // A-SNS wants to send us an RS485 message for occupancy sensor change
    MsgOutgoing[RS485_LEN_OFFSET] = 5;
    MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_SNS;
    MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;
    MsgOutgoing[3] = 'S';  // Sensor request message
    MsgOutgoing[4] = calcChecksumCRC8(MsgOutgoing, 4);
    // Out goes the message!
    RS485SendMessage(MsgOutgoing);
    // Now do nothing but wait for a response from A-SNS...
    bool forUs = false;
    do {
      if (RS485GetMessage(MsgIncoming)) {
        if (MsgIncoming[RS485_TO_OFFSET] == ARDUINO_MAS) {
          forUs = true;
        }
      }
    } while (!forUs);
    // Now we have an RS485 message sent to A_MAS.  Just for fun, let's confirm that it's from A_SNS, and that we
    // have the appropriate "request" command we are expecting...
    // If not coming from A_SNS or not an 'S'-type (sensor status) message, something is wrong.
    if ((MsgIncoming[RS485_FROM_OFFSET] != ARDUINO_SNS) || (MsgIncoming[3] != 'S')) {
      sprintf(lcdString, "Unexpected A-SNS msg");
      LCD2004.send(lcdString);
      Serial.print(lcdString);
      endWithFlashingLED(6);
    }
    // Yay.  We have a new sensor status from A-SNS.
    // INTERESTING: We apparently don't use the * dereference operator if we passed an array by reference...
    * tNum = MsgIncoming[4];     // Sensor number
    * tStatus = MsgIncoming[5];  // 0 if cleared, 1 if tripped
    return true;  // Yes, we got a new sensor status array
  }
  return false;   // No, we did not get a new sensor status array
}

void checkIfRequestToStartNewMode() {
  // Let's see if the operator wants to start a mode...we don't need to return anything to the calling main loop().
  // Only called when we are in STOPPED state.  We wont do anything but wait for operator to Start a new valid state (and check emergency stop.)
  // The "STOPPED" code could be a "while" loop but that will prevent us from continuing to look at potential other
  // things that we might want to be doing in A-MAS, not necessarily related to mode status.
  // i.e. an "if" block is non-blocking, versus a "while" block is blocking.
  // This code assumes that when a mode is stopped, it's green LED will be turned off.  So all five mode LEDs
  // should be off when this loop is *first* entered after STOPPING mode.
  // As the operator turns the mode dial, the Start button LED (and not the mode dial LED) will illuminate to signal if a mode can be started.
  // When STOPPED, all green mode LEDs will be off until Start is pressed and a new mode is started.
  // The red Stop LED is always off because Stop is not an option when you are already stopped.
  // We will light the green Start button LED when (and only when) the selector is turned to a mode that
  // can next be started.  I.e. you can't start Auto mode if you just stopped Manual mode.
  // Once a valid mode has been selected, and the illuminated Start button pressed, the Start light will
  // go out, the new green mode LED will be lit, the Stop light will illuminate, and this code is done.
  // Except, be sure to "broadcast" via RS485 if we enter a new mode, which is only possible here.
  // Look at the rotary switch and respond when turned to a valid new mode.  All we will do is illuminate
  // the Start button when the rotary switch is pointing at a valid option, and see if they also press Start.
  if (digitalRead(PIN_ROTARY_MANUAL) == LOW) {           // Selector dial point at Manual
    // When stopped, you can start Manual mode regardless of what the previous mode was, so turn on Start LED
    digitalWrite(PIN_ROTARY_LED_START, LOW);             // Illuminate the Start button since it could be pressed now
    if (digitalRead(PIN_ROTARY_START) == LOW)  {         // Is the operator pressing the Start button at this moment?
      modeCurrent = MODE_MANUAL;                         // Okay, then start Manual mode!
      stateCurrent = STATE_RUNNING;
      digitalWrite(PIN_ROTARY_LED_MANUAL, LOW);          // Turn on the Manual mode LED to show it's now the active mode
    }
  }
  else if (digitalRead(PIN_ROTARY_REGISTER) == LOW) {    // Selector dial is turned to Register
    // When stopped, you can start Register mode regardless of what the previous mode was
    digitalWrite(PIN_ROTARY_LED_START, LOW);             // Illuminate the Start button since it could be pressed now
    if (digitalRead(PIN_ROTARY_START) == LOW)  {         // Is the operator pressing the Start button at this moment?
      modeCurrent = MODE_REGISTER;                       // Okay, then start Register mode
      stateCurrent = STATE_RUNNING;
      digitalWrite(PIN_ROTARY_LED_REGISTER, LOW);        // Turn on the Register mode LED to show it's now the active mode
    }
  }
  else if (digitalRead(PIN_ROTARY_AUTO) == LOW) {        // Selector dial is turned to Auto
    // We will ONLY allow Auto mode if previous mode was Register, Auto, or Park, and that mode was stopped gracefully.
    // The system will not allow the operator to STOP during registration; they must complete it or press Halt or Reset to restart everything.
    if ((modeCurrent == MODE_REGISTER) || (modeCurrent == MODE_AUTO) || (modeCurrent == MODE_PARK)) {   // Okay to turn on Start LED and accept press if pressed
      digitalWrite(PIN_ROTARY_LED_START, LOW);           // Illuminate the Start button since it could be pressed now
      if (digitalRead(PIN_ROTARY_START) == LOW) {        // Is the operator pressing the Start button at this moment?
        modeCurrent = MODE_AUTO;                         // Okay, then start Auto mode
        stateCurrent = STATE_RUNNING;
        digitalWrite(PIN_ROTARY_LED_AUTO, LOW);          // Turn on the Auto mode LED to show it's now the active mode
      }
    }
  }
  else if (digitalRead(PIN_ROTARY_PARK) == LOW) {        // Selector dial is turned to Park
    // We can ONLY allow Park mode if previous mode was Register, Auto, or Park.
    if ((modeCurrent == MODE_REGISTER) || (modeCurrent == MODE_AUTO) || (modeCurrent == MODE_PARK)) {   // Okay to turn on Start LED and accept press if pressed
      digitalWrite(PIN_ROTARY_LED_START, LOW);           // Illuminate the Start button since it could be pressed now
      if (digitalRead(PIN_ROTARY_START) == LOW) {        // Is the operator pressing the Start button at this moment?
        modeCurrent = MODE_PARK;                         // Okay, then start Park mode
        stateCurrent = STATE_RUNNING;
        digitalWrite(PIN_ROTARY_LED_PARK, LOW);          // Turn on the Register mode LED to show it's now the active mode
      }
    }
  }
  else {  // They have not moved the selector switch to a position where we could allow them to press Start
    digitalWrite(PIN_ROTARY_LED_START, HIGH);             // Turn off the Start button LED since it could NOT be pressed now
  }
  // We arrive here starting when stateCurrent is STOPPED, and we *possibly* changed it to start a mode.  If we did, then stateCurrent
  // will now be set to RUNNING and modeCurrent will be our new mode.
  // So if stateCurrent is now RUNNING, then we need to turn off the Start button LED, and turn on the Stop button LED.
  // We also need to set the firstTimeThrough flag back to true, since we are new to this mode.
  // We also need to do an RS485 broadcast to let everyone know we have a new mode.
  if (stateCurrent == STATE_RUNNING) {    // This is true only if we just selected a new mode after being stopped
    digitalWrite(PIN_ROTARY_LED_START, HIGH);        // Turn off the Start button LED since we have accepted this new mode
    digitalWrite(PIN_ROTARY_LED_STOP, LOW);          // Turn on the Stop button LED so operator knowns they can use it now
    firstTimeThrough = true;           // Set true EVERY "first time" into the main Manual/Register/Auto/Park "do-the-mode" code after previously stopped
    sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
  }
  return;
}

void checkIfRequestToStopMode() {
  // See if we need to transition from RUNNING to STOPPING (but not yet STOPPED)  This potentially just sets the new mode to STOPPING.
  // When running, we don't care what happens to the mode selector switch, we just have the current mode illuminated
  // and the Stop button illuminated.  The only action available is to press the Stop button at this point.
  // And the Stop button should ALWAYS be illuminated when in an active running mode.
  // May need special consideration if trying to stop Auto or Park mode i.e. set mode to Manual if Auto/Park stopped prematurely.
  // Check to see if the Stop button has been pressed (it should be illuminated when in any running mode)
  // If so, bring the current mode to a controlled stop BEFORE starting to monitor for selection of a new mode.
  if (digitalRead(PIN_ROTARY_STOP) == LOW) {     // Is the operator pressing the Stop button at this moment?
    stateCurrent = STATE_STOPPING;              // Okay, then initiate stopping of the current mode, whatever it is
    digitalWrite(PIN_ROTARY_LED_STOP, HIGH);     // And turn off the Stop button LED to acknowledge stop button press accepted
    sendRS485ModeBroadcast(modeCurrent, stateCurrent); // Tell everyone via RS485 about our new mode/state
  }
  return;
}

void FRAM1UpdateLastKnown(const byte t, const byte b, const char d) {
  lastTrainLoc[t - 1].blockNum = b;
  lastTrainLoc[t - 1].whichDirection = d;
  updateFRAM1ControlBlock();     // We could wait and do this after all messages have been received, below, but this make it more clear.
  return;
}

// **************************************************************
// ********************** RS485 FUNCTIONS ***********************
// **************************************************************

void RS485fromMAStoSWT_SetLastKnown() {
  sprintf(lcdString, "%.20s", "Last-known turnouts.");
  LCD2004.send(lcdString);
  Serial.println(lcdString);
  MsgOutgoing[RS485_LEN_OFFSET] = 9;   // Length is 9 bytes: Length, To, From, 'L', 4 bytes data, CRC
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_SWT;  // Byte 1.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2.
  MsgOutgoing[3] = 'L';   // Command to A-SWT = 'L' = Last-known turnout position
  for (byte i = 0; i < 4; i++) {   // i represents each of the four bytes of turnout data
    MsgOutgoing[i + 4] = lastKnownTurnout[i];
  }
  MsgOutgoing[8] = calcChecksumCRC8(MsgOutgoing, 8); // Add CRC8 checksum
  RS485SendMessage(MsgOutgoing);
  return;
}

bool RS485fromOCCtoMAS_Reply() {
  if (MsgIncoming[RS485_TO_OFFSET] == ARDUINO_MAS) {        // Okay so far...
    if (MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_OCC) {    // Okay so far...
      if (MsgIncoming[3] == 'Q') {  // Q means it's Reply to a question prompt so we are golden!
        return true;
      }
    }
  }
  return false;
}

bool RS485fromOCCtoMAS_RegisteredTrain() {
  if (MsgIncoming[RS485_TO_OFFSET] == ARDUINO_MAS) {        // Okay so far...
    if (MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_OCC) {    // Okay so far...
      if (MsgIncoming[3] == 'T') {  // T means it's Train data (what we expect) so we are golden!
        return true;
      }
    }
  }
  return false;
}

bool RS485AskOCCtoPromptForSmoke() {
  Serial.println("Sending RS485 to A-OCC to prompt for smoke y/n...");
  MsgOutgoing[RS485_LEN_OFFSET] = 14;   // Byte 0 is length of message
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_OCC;  // Byte 1 is always A-OCC.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is always A-MAS.
  MsgOutgoing[3] = 'Q';   // Byte 3 = Q for Query
  sprintf(occPrompt,"SMOKE  N");  // First of two Smoke prompts
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'N';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  delay(50);   // Brief delay so we don't overflow A-OCC incoming serial buffer
  sprintf(occPrompt,"SMOKE  Y");  // Second of two Smoke prompts
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'Y';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  // Now wait until A-OCC sends us a reply of 0 or 1 (No or Yes per the above two smoke prompts)
  while (RS485GetMessage(MsgIncoming) == false) {  }   // Wait for the expected RS485 incoming reply
  if (!RS485fromOCCtoMAS_Reply()) {  // If *not* A-OCC is replying to a question sent by A-MAS such as Smoke Y/N, etc.
    sprintf(lcdString, "%.20s", "Smoke fatal error!");
    LCD2004.send(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  if (MsgIncoming[4] == 0) {
    return false;  // false means no smoke, true means yes smoke
  } else {
    return true;
  }
}

void RS485TellLEGifUseSmoke(const bool i) {          // Now tell A-LEG if we want smoke or not...
  MsgOutgoing[RS485_LEN_OFFSET] = 6;   // Byte 0 is length of message
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_LEG;  // Byte 1 is "to"
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is "from"
  MsgOutgoing[3] = 'S';   // Byte 3 = S for Smoke
  if (i == false) {   // false means No smoke, true means Yes smoke
    MsgOutgoing[4] = 'N';
    smokeOn = false;
    Serial.println("Smoke OFF");
  } else {        // Must be 1 means Yes smoke
    MsgOutgoing[4] = 'Y';
    smokeOn = true;
    Serial.println("Smoke ON");
  }
  MsgOutgoing[5] = calcChecksumCRC8(MsgOutgoing, 5); 
  RS485SendMessage(MsgOutgoing);  
  return;
}

bool RS485AskOCCtoPromptStartupSpeed() {
  Serial.println("Sending RS485 to A-OCC to prompt for fast or slow startup...");
  MsgOutgoing[RS485_LEN_OFFSET] = 14;   // Byte 0 is length of message
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_OCC;  // Byte 1 is always A-OCC.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is always A-MAS.
  MsgOutgoing[3] = 'Q';   // Byte 3 = Q for Query
  sprintf(occPrompt,"FAST ON ");  // First of two startup prompts
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'N';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  delay(50);   // Brief delay so we don't fill A-OCC incoming serial buffer too quickly
  sprintf(occPrompt,"SLOW ON ");  // Second of two startup prompts
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'Y';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  // Now wait until A-OCC sends us a reply of 0 or 1 (Fast or SLow per the above two startup prompts)
  while (RS485GetMessage(MsgIncoming) == false) {  }   // Wait for the expected RS485 incoming reply
  if (!RS485fromOCCtoMAS_Reply()) {  // If *not* A-OCC is replying to a question sent by A-MAS such as Smoke Y/N, etc.
    sprintf(lcdString, "%.20s", "Startup fatal error!");
    LCD2004.send(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  if (MsgIncoming[4] == 0) {
    return false;  // false means fast startup, true means slow startup
  } else {
    return true;
  }

  return MsgIncoming[4];  // 0 means fast startup, 1 means slow startup
}

void RS485TellLEGStartupSpeed(const bool i) {          // Now tell A-LEG if we want fast or slow startup of locos...
  MsgOutgoing[RS485_LEN_OFFSET] = 6;   // Byte 0 is length of message
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_LEG;  // Byte 1 is "to"
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is "from"
  MsgOutgoing[3] = 'F';   // Byte 3 = F for Fast or Slow startup
  if (i == false) {   // false means fast startup, true means slow startup
    MsgOutgoing[4] = 'F';
    slowStartup = false;
    Serial.println("Fast startup.");
  } else {        // Must be 1 means slow startup
    MsgOutgoing[4] = 'S';
    slowStartup = true;
    Serial.println("Slow startup.");
  }
  MsgOutgoing[5] = calcChecksumCRC8(MsgOutgoing, 5); 
  RS485SendMessage(MsgOutgoing);
  return;
}

bool RS485AskOCCtoPromptAnnouncements() {
  Serial.println("Sending RS485 to A-OCC to prompt for audio PA system or not...");
  MsgOutgoing[RS485_LEN_OFFSET] = 14;   // Byte 0 is length of message
  MsgOutgoing[RS485_TO_OFFSET] = ARDUINO_OCC;  // Byte 1 is always A-OCC.
  MsgOutgoing[RS485_FROM_OFFSET] = ARDUINO_MAS;  // Byte 2 is always A-MAS.
  MsgOutgoing[3] = 'Q';   // Byte 3 = Q for Query
  sprintf(occPrompt,"AUDIO N ");  // First of two prompts.  AUDIO must be upper-case so the A-OCC code recognizes it (if it even needs to?)
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'N';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  delay(50);   // Brief delay so we don't fill A-OCC incoming serial buffer too quickly
  sprintf(occPrompt,"AUDIO Y ");  // Second of two prompts
  memcpy(MsgOutgoing + 4, occPrompt, 8);
  MsgOutgoing[12] = 'Y';   // Last record?
  MsgOutgoing[13] = calcChecksumCRC8(MsgOutgoing, 13); 
  RS485SendMessage(MsgOutgoing);  
  // Now wait until A-OCC sends us a reply of 0 or 1 (Use the PA Audio announcement system, or not.)
  while (RS485GetMessage(MsgIncoming) == false) {  }   // Wait for the expected RS485 incoming reply
  if (!RS485fromOCCtoMAS_Reply()) {  // If *not* A-OCC is replying to a question sent by A-MAS such as Smoke Y/N, etc.
    sprintf(lcdString, "%.20s", "Startup fatal error!");
    LCD2004.send(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(3);
  }
  if (MsgIncoming[4] == 0) {   // 0 means no audio PA, 1 means use audio PA
    return false;
    Serial.println("Not using PA system.");
  } else {        // Must be 1 means use audio PA
    return true;
    Serial.println("PA system will be used.");
  }
}

// 9/30/17: THIS FUNCTION SHOULD PROBABLY PASS NOTHING AND RETURN A 5-BYTE STRUCT (Train, Block, Dir, Entry, Exit)
void RS485fromOCCtoMAS_ExtractData(byte * t, byte * b, char * d, byte * n, byte * x) {
  // Returns train number, block number, direction, entry sensor, and exit sensor, using data in MsgIncoming.
  // Could be re-written to pass nothing and return a 5-byte struct...???
  * t = MsgIncoming[4];  // Train number: 1..8 -- we must assume that any other block is static (train 99)
  * b = MsgIncoming[5];  // Block number 1..32 that the train is in
  * d = MsgIncoming[6];  // Direction E or W that the train is facing
  // RS485 data does not include entry and exit sensors; we must look those up:
  getEntryAndExitSensorFromBlockAndDir(* b, * d, n, x);  // Same as (* b, * d, & * n, & * x)
  return;
}

// 9/30/17: THIS FUNCTION SHOULD PROBABLY PASS BLOCK and DIR (maybe as a 2-byte struct), and RETURN A 2-BYTE STRUCT (Entry, Exit pair)
void getEntryAndExitSensorFromBlockAndDir(const byte b, const char d, byte * n, byte * x) {
  // Given a block number 1..52 and direction E or W, return the corresponding entry (n) and exit (x) sensor numbers
  if (d == 'E') {
    * n = blockSensor[b - 1].eastEntryWestExit;  // Entry sensor
    * x = blockSensor[b - 1].westEntryEastExit;  // Exit sensor
  } else if (d == 'W') {
    * n = blockSensor[b - 1].westEntryEastExit;  // Entry sensor
    * x = blockSensor[b - 1].eastEntryWestExit;  // Exit sensor
  } else {
    sprintf(lcdString, "Bad Reg Sensor!");
    LCD2004.send(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }
  return;
}

bool RS485fromOCCtoMAS_TrainRecord() {    // N means that this is a train data record; otherwise it's an "all done" record with no train data.
  return (MsgIncoming[7] == 'N');
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
    LCD2004.send(lcdString);
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

// ***************************************************************************
// *** HERE ARE FUNCTIONS USED BY VIRTUALLY ALL ARDUINOS *** REV: 09-12-16 ***
// ***************************************************************************

void RS485SendMessage(byte * tMsg) {  // byte tMsg[] equivalent to byte * tMsg; take your pick
  // 10/1/16: Updated from 9/12/16 to write entire message as single Serial2.write(msg,len) command.
  // This routine must *only* be called when an entire message is ready to write, not a byte at a time.
  digitalWrite(PIN_RS485_TX_LED, HIGH);       // Turn on the transmit LED
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_TRANSMIT);  // Turn on transmit mode
  Serial2.write(tMsg, tMsg[0]);  // tMsg[0] is always the number of bytes in the message, so write that many bytes
  // Serial2.write((unsigned char*) tMsg, tMsg[0]);  // tMsg[0] is always the number of bytes in the message, so write that many bytes
  // March 2018: Added (unsigned char *) to the above Serial.write() command, per Tim M., to fix a red squiggly.
  // April 2018: Must have been fixed, because original version works again, and 'unsigned char*' version gives red squiggly!
  // See emails between Tim and I on 3/21/18 to 3/22/18.
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
  // Input byte tMsg[] is the initialized incoming byte array whose contents may be filled.
  // tMsg[] is also "returned" by the function since arrays are passed by reference.
  // If this function returns true, then we are guaranteed to have a real/accurate message in the
  // buffer, including good CRC.  However, the function does not check if it is to "us" (this Arduino) or not.

  byte tMsgLen = Serial2.peek();     // First byte will be message length
  byte tBufLen = Serial2.available();  // How many bytes are waiting?  Size is 64.
  if (tBufLen >= tMsgLen) {  // We have at least enough bytes for a complete incoming message
    digitalWrite(PIN_RS485_RX_LED, HIGH);       // Turn on the receive LED
    if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too short!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tMsgLen > RS485_MAX_LEN) {            // Message too long to be any real message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too long!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tBufLen > 60) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 in buf ovrflw!");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    for (int i = 0; i < tMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the tMsg[] byte array
      tMsg[i] = Serial2.read();
    }
    if (tMsg[tMsgLen - 1] != calcChecksumCRC8(tMsg, tMsgLen - 1)) {   // Bad checksum.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 bad checksum!");
      LCD2004.send(lcdString);
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
  // chip rev number matches code rev number, and get last-known-position-and-direction of all trains structure.

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
  // We need to put turnouts in a known starting state, so get last known orientation of each turnout from previous operating session
  for (byte i = 0; i < 4; i++) {
    lastKnownTurnout[i] = FRAM1ControlBuf[(3 + i)];
  }
  
  // Now get last known position of each train from previous operating session
  for (byte i = 0; i < MAX_TRAINS; i++) {
    byte j = 7 + (i * 3);  // Offset into FRAM1 control buffer
    lastTrainLoc[i].trainNum = FRAM1ControlBuf[j];        // 1..10
    lastTrainLoc[i].blockNum = FRAM1ControlBuf[j + 1];    // 1..26
    lastTrainLoc[i].whichDirection = FRAM1ControlBuf[j + 2];  // E|W
  }

  Serial.println(F("Here are the last-known train numbers, blocks, and directions retrieved from control block:"));
  for (byte i = 0; i < MAX_TRAINS; i++) {
    Serial.print(lastTrainLoc[i].trainNum); Serial.print(", ");
    Serial.print(lastTrainLoc[i].blockNum); Serial.print(", '");
    Serial.print(lastTrainLoc[i].whichDirection); Serial.println("'");
  }
  return;
}

void updateFRAM1ControlBlock() {
  // 10/19/16: This is really an A-MAS-only function.
  // Try to call this function whenever a train location or turnout orientation changes.
  // We must populate/update lastKnownTurnout[] and lastTrainLoc[] *before* calling this routine.
  // FRAM1ControlBuf will already be populated to preserve the revision date (first 3 bytes.)
  // First insert current turnout positions into control block
  memcpy(FRAM1ControlBuf + 3, &lastKnownTurnout, sizeof(lastKnownTurnout));
  // Now insert last known train positions into control block
  memcpy(FRAM1ControlBuf + 7, &lastTrainLoc, sizeof(lastTrainLoc));
  // Now write the FRAM1 control buffer
  FRAM1.writeControlBlock(FRAM1ControlBuf);
  return;
}

/*
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
  //   LCD2004.send(lcdString);   i.e. "Hello world."
  //   int a = 7; unsigned long t = millis(); char c = 'R';
  //   sprintf(lcdString, "I %3i T %6lu C %3c", a, t, c);  Will also crash if longer than 20 chars!
  //   LCD2004.send(lcdString);   i.e. "I...7.T...3149.C...R"
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
*/

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
      LCD2004.send(lcdString);
      Serial.println(lcdString);
      while (true) { }  // For a real halt, just loop forever.
    } else {
      sprintf(lcdString,"False HALT detected.");
      LCD2004.send(lcdString);
      Serial.println(lcdString);
    }
  }
  return;
}
