// A-LEG Rev: 10/01/17.
char APPVERSION[21] = "A-LEG Rev. 10/01/17";

// Include the following #define if we want to run the system with just the lower-level track.  Comment out to create records for both levels of track.
#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

// Include the following #define if we want to populate the Delayed Action table with some test data, versus initializing empty.
#define TEST_DATA        // Comment this out for non-test scenario, normal version.

// Need to re-write (maybe) Legacy command buffer (circular buffer similar to turnout buffer?)
// Need to re-write Train Progress table from array to structure, and revise circular buffer logic.
// Need to revise some inline code to functions.
// Need to consider sending some parameters to functions by reference (other than globals.)
// GOOD IDEA: If smoke is on, how about a command to turn OFF smoke on all locos if they have been running for more than 10 minutes?



// 09/16/17: Changed variable suffixes from Length to Len, No/Number to Num, Record to Rec, etc.
// 02/29/17: Adding logic for Auto mode, and populating and processing Delayed Action and Train Progress tables.
// 02/26/17: Added mode and state changed = false at top of get RS485 message main loop
// 02/25/17: Adding code to watch Registration messages between A-MAS and A-OCC, and act accordingly.

// NOTE: We *do* care about Mode Change messages from A-MAS: Entering Registration mode, we need to clear out Train Progress and Delayed
// action tables.  And although we always track sensor changes, if we are not in Auto or Park mode, we will not scan the Train Progress
// or Delayed Action tables when sensor changes are received.
// When we receive RS485 route commands from A-MAS, we will see if they are regular 'R'oute, or Park 1, or Park 2.  So we will need some special
// logic for Park 2 to insert "put loco in reverse" and also ignore sensor 36, and set predetermined speed (maybe loco "slow") versus regular
// speed for Block 9 (which we won't see anyway since it's not listed in the Park 2 table.)
// Hopefully, A-MAS won't send us a Park 1 command, or even stop Auto mode, until it sees that all trains have completed their Auto mode and cleared
// all sensors except destination exit sensor (i.e. presumably they are all stopped.)
// A-MAS will allow operator to press the Stop button in Auto mode, but will not illuminate the Start button for Park mode until all Auto trains
// have completed their routes.
// A-LEG can assume that if it receives a Park 1 route, that all Auto trains have stopped and Train Progress shows only one block/train.
// If the operator needs to stop trains abnormally, he can push Halt or use the control panel PowerMaster off switches, which are always monitored.
// Duties of A-LEG:
// 1. Monitor the Halt line and send an immediate Emergency Stop command to Legacy if pulled low (regardless of mode.)
// 2. Monitor the control panel PowerMaster toggle switches and turn them on and off if operator moves one of them (regardless of mode.)
// 3. Monitor the RS485 bus for MODE CHANGE or STATE CHANGE commands from A-MAS.  
//      When REGISTER mode is started, we should clear out the Train Progress and Delayed Action tables, as well as the Sensor
//      Status and Block Reservation tables, if that is relevant here (Not sure).  i.e. reset everything.
// 4. Watch for RS485 commands from A-MAS during Registration, including SMOKE Y/N, and STARTUP FAST/SLOW.
// 5. Watch for RS485 commands from A-MAS (or A-OCC to A-MAS, haven't decided as of 9/27/17) during Registration for TRAIN REGISTERED.
//      As each train is registered, A-LEG will create a new initial record in the Train Progress table for that train, turn smoke on or off,
//      and run the startup sequence.
// The following Train Control duties are only going to happen when in Auto or Park mode:
// 6. Watch for RS485 New Route commands from A-MAS.  This will include a Train Num. (1 thru 10) and a Route Num. (Regular, or Park 1 or Park 2.)
//    A-MAS implies okay to proceed with a train when it sends a route; A-LEG can decide if it should wait a bit, or not.
//    When a new route is identified, populate the Delayed Action table with a set of Timed records to make announcements, blow whistle, start train,
//    etc., and also a set of Sensor records to be executed as the train moves along the route.
// 7. Watch for RS485 Sensor Change messages from A-SNS.
//    In AUTO and PARK modes, when a sensor-change message is received, immediately scan Delayed Action for relevant records, do whatever is
//    indicated there.  This could be, for instance, turn off sound when entering a tunnel, start or stop a crossing signal, etc.
//    If the sensor-change is a Destination Entry or Destination Exit, run code to populate the Delayed Action table with commands to slow train, turn
//    on bell, stop the train, etc.  We don't populate the Delayed Action table with ANY commands for the destination block at the time that we populate
//    to start or continue a route, because we won't know until later if we might get a "continue on with another route" command from A-MAS before we
//    start to slow down -- so we may never need to execute slow-and-stop commands for the original destination siding.
// 8. Scan the Delayed Action table (in FRAM2) looking for records that are "ripe" based on time, or when a sensor event has occurred.
//    When Delayed Action table records are found that need to be executed, they will be inserted into the Legacy Command buffer.
// 9. Check for any commands in the Legacy Command buffer, and execute them as soon as possible.

// Here are the data and types of commands that we will need to know about:
//   Device Type(1 char) (Engine, Train, Accessory, Switch, Route [E|T|A|S|R]
//   Device Num (1 byte) (engine, train, accessory, switch, or route number)[0..99]
//     NOTE: We may implement Switch and Route commands here if we want to support Parking trains including backing into a siding.
//     Should actually be pretty straightforward to implement but we need other Arduinos involved.
//   Command Type(1 char)[E | A | M | S | B | D | F | C | T | Y | W | R]
//     E = Emergency Stop All Devices(FE FF FF) (Note : all other parms in command are moot so ignore them)
//     A = Absolute Speed command
//     M = Momentum 0..7
//     S = Stop Immediate(this engine / train only)
//     B = Basic 3 - byte Legacy command(always AAAA AAA1 XXXX XXXX)
//     D = Railsounds Dialogue(9 - byte Legacy extended command)
//     F = Railsounds Effect(9 - byte Legacy extended command)
//     C = Effect Control(9 - byte Legacy extended command)
//     T = TMCC command
//     Y = Accessory.  Parm 1 will be 0=off, 1=on       (Custom relay board; not sent to Legacy/TMCC)
//     W = Switch.  Parm 1 will be 0=Normal, 1=Reverse  (UNUSED in any foreseeable version)
//     R = Route.  Parm 1 will be route number to set.  (UNUSED in any foreseeable version)
//   Command Parameter (1 byte) (speed, momentum, etc., or hex value from Legacy protocol tables)
// PowerMasters are controlled by addressing their Engine Num. and setting their Abs Speed to 1 (on) or 0 (off.)

// **************************************************************************************************************************

// (Comments from loop() code below...
// We need to watch for the following RS485 messages:
//   In ANY mode:
//     From A-MAS to ALL broadcasting change of mode and state (i.e. Register Mode, Running)
//     From A-SNS to A-MAS reporting a sensor status change.
//   In REGISTER mode, RUNNING state:  IMPORTANT: Sensor status MUST NOT change during Registration, all trains must be stopped!
//       We can add logic to check before-and-after status of every sensor, but for now we'll just assume that we don't want to crash the program.
//     From A-OCC to A-MAS certain replies from the operator via a/n display.  i.e. SMOKE Y/N, STARTUP FAST/SLOW.
//     From A-OCC to A-MAS replies with operator registration data.  A-LEG should establish a fresh Train Progress record and startup the loco.
//   In AUTO or PARK mode, RUNNING or STOPPING state:
//     From A-MAS to A-LEG assigning a new route (or Park 1 or Park 2?) to a train (will not occur if state = STOPPING)
//     From A-MAS to ALL, changing state to STOPPING -- probably nothing special to do until another mode started.
//     From A-MAS to ALL, changing state to STOPPED -- continue to maintain Train Progress in case we start Auto again???
//       NOTE: A-LEG does not care about block reservations, so nothing to do there -- but we do use the table for reference to block data.

// *** RS485 MESSAGE PROTOCOLS used by A-LEG.  Byte numbers represent offsets, so they start at zero. ***
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

// A-SNS to A-MAS:  Sensor status update for a single sensor change (A-LEG snoops directly, and updates Train Progress etc.)
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 7
//    1	   To	       Byte	 1 (A_MAS)
//    2	   From	     Byte	 3 (A_SNS)
//    3    Command   Char  'S' Sensor status update
//    4    Sensor #  Byte  1..52 (Note that as of Sept 2017, we have disconnected sensor 53 from the layout)
//    5    Trip/Clr  Byte  [0|1] 0-Cleared, 1=Tripped
//    6	   Checksum	 Byte  0..255

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

// A-MAS to A-LEG:  Command to set a new Route (regular, or Park 1 or Park 2) or a registered train.  AUTO and PARK MODE only.
// Rev: 08/31/17
// OFFSET  DESC      SIZE  CONTENTS
//    0	   Length	   Byte	 7
//    1	   To	       Byte	 2 (A_LEG)
//    2	   From	     Byte	 1 (A_MAS)
//    3    Command   Char  [T|1|2] rouTe|park 1|park 2 ('T' for consistency with A-SWT protocols.)
//    4    Route Num Byte  Route number from one of the three route tables, 1..70, 1..19, or 1..4
//    5    Train Num Byte  1..9
//    6	   Checksum	 Byte  0..255

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

// *** ARDUINO PIN NUMBERS: Define Arduino pin numbers used...specific to A-LEG
const byte PIN_RS485_TX_ENABLE =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
const byte PIN_RS485_TX_LED    =  5;  // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
const byte PIN_RS485_RX_LED    =  6;  // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
const byte PIN_SPEAKER         =  7;  // Output: Piezo buzzer connects positive here
const byte PIN_REQ_TX_A_LEG    =  8;  // Output pin pulled LOW by A-LEG when it wants to send message to A-MAS
const byte PIN_HALT            =  9;  // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF
const byte PIN_FRAM1           = 11;  // Output: FRAM1 chip select.  Route Reference table and last-known-state of all trains.
const byte PIN_FRAM2           = 12;  // Output: FRAM2 chip select.  Event Reference and Delayed Action tables.
const byte PIN_LED             = 13;  // Built-in LED always pin 13
const byte PIN_FRAM3           = 19;  // Output: FRAM3 chip select.
const byte PIN_PANEL_BROWN_ON  = 23;  // Input: Control panel "Brown" PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_BROWN_OFF = 25;  // Input: Control panel "Brown" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_BLUE_ON   = 27;  // Input: Control panel "Blue" PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_BLUE_OFF  = 29;  // Input: Control panel "Blue" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_RED_ON    = 31;  // Input: Control panel "Red" PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_RED_OFF   = 33;  // Input: Control panel "Red" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_4_ON      = 35;  // Input: Control panel #4 PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_4_OFF     = 37;  // Input: Control panel #4 PowerMaster toggled down.  Pulled LOW.

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
byte               FRAM2ControlBuf[FRAM_CONTROL_BUF_SIZE];
unsigned int       FRAM2BufSize             =   0;  // We will retrieve this via a function call, but it better be 128!
unsigned long      FRAM2Bottom              =   0;  // Should be 128 (address 0..127)
unsigned long      FRAM2Top                 =   0;  // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)
const byte         FRAM2VERSION[3]          = {  9, 16, 17 };  // This must match the version date stored in the FRAM2 control block.
byte               FRAM2GotVersion[3]       = {  0,  0,  0 };  // This will hold the version retrieved from FRAM2, to confirm matches version above.
const byte         FRAM2_ACTION_START       = 128;  // Address in FRAM2 memory to start writing our Delayed Action table (0..127 is the control block, not currently used in FRAM2)
const byte         FRAM2_ACTION_LEN         =  12;  // Each record in the Delayed Action table is 12 bytes long
Hackscribble_Ferro FRAM2(MB85RS64, PIN_FRAM2);  // Create the FRAM2 object!
// Repeat above for FRAM3 if we need a third FRAM

// *** MISC CONSTANTS AND GLOBALS: needed by A-LEG:
// true = any non-zero number
// false = 0

const byte TOTAL_SENSORS           =  52;
const byte TOTAL_BLOCKS            =  26;
const byte MAX_TRAINS              =   8;
const byte TRAIN_ID_NULL           =   0;  // Used for "no train."
const byte TRAIN_ID_STATIC         =  99;  // This train number is a static train.
const byte MAX_BLOCKS_PER_TRAIN    =  12;  // Maximum number of occupied blocks FOR ANY ONE TRAIN in the Train Progress table, to dimension array.
const byte TOTAL_TURNOUTS          =  32;  // 30 connected, but 32 relays.

// Global constants and variables used in Legacy/TMCC serial command buffer.
// First define global bytes for each of the 9 possible used in Legacy commands.
// Most commands only use simple 3-byte Legacy (or TMCC) commands.
// Nine-byte commands are for Dialogue, Fx, and Control "extended" commands.
byte legacy11 = 0x00;   // F8 for Engine, or F9 for Train (or FE for emergency stop all or TMCC)
byte legacy12 = 0x00;
byte legacy13 = 0x00;
byte legacy21 = 0xFB;   // Always FB for 2nd and 3rd "words", when used
byte legacy22 = 0x00;
byte legacy23 = 0x00;
byte legacy31 = 0xFB;   // Always FB for 2nd and 3rd "words", when used
byte legacy32 = 0x00;
byte legacy33 = 0x00;   // This will hold checksum for "multi-word" commands

const byte LEGACY_REPEATS = 3;                  // How many times to re-transmit every 3-byte command to ensure reliability.
  // NOTE: LEGACY_REPEATS should be implemented as each command is sent to the Legacy Command buffer, so that mission-critical things like absolute
  // speed, halt, momentum, etc. will be repeated multiple times; but special effects such as blow horn, play dialogue, increase volume, etc. will
  // only be transmitted once.  Transmitting things like blow horn would probably not work properly if sent three times, for example.
const unsigned long LEGACY_MIN_INTERVAL_MS =  25;  // Minimum number of milliseconds between subsequent Legacy commands.
  // LEGACY_LATENCY is used when calculating train deceleration, on how far the train will travel after hitting a siding entry sensor, until
  // it receives a command from Legacy to slow to a given speed at a given momentum.  Our initial default is 2.5 seconds.
  // If all trains seem to slow to crawl too soon before hitting the siding exit sensor, then we can reduce this value, and it will
  // automatically be incorporated into the deceleration calculations.
  // If all trains seem to overshoot the exit sensor, then increase this global value.
  // If only some trains do not hit crawl speed at the desired location, then update their individula deceleration data.
  // IMPORTANT: It might be that this needs to change as a factor of the number of trains currently running -- more trains mean more latency.  This would
  // be easy to implement by adjusting each time the Train Progress table gains or loses a train, and make this a variable not a constant.
const unsigned int LEGACY_LATENCY_MS = 2500;   // How many milliseconds after train trips a sensor, until it receives a command from Legacy.
const byte LEGACY_BUF_ELEMENTS       =  100;   // Elements 0..99.  Increase if error buffer overflow.  i.e. 100 elements holds 33 3-byte commands.
byte legacyCmdBuf[LEGACY_BUF_ELEMENTS];            // Buffer for Legacy/TMCC commands
byte legacyCmdBufHead                =    0;   // Next array element to be written
byte legacyCmdBufTail                =    0;   // Next array element to be removed
byte legacyCmdBufCount               =    0;   // Num active elements in buffer.  Max is LEGACY_BUF_ELEMENTS.
const byte POWERMASTER_1_ID          =   91;   // This is the Engine Number needed by Legacy to turn PowerMasters on and off.
const byte POWERMASTER_2_ID          =   92;   // These can be changed by re-programming the PowerMasters, and changing these constants.
const byte POWERMASTER_3_ID          =   93;
const byte POWERMASTER_4_ID          =   94;   // Not using this one as of Jan 2017.
const byte MOMENTUM_DEPARTING        =    6;   // Legacy momentum setting to use when pulling out of a station
const byte MOMENTUM_CHANGING         =    4;   // Legacy momentum setting to use when changing speed from one block to the next (except destination when stopping.)
const byte SMOKE_OFF                 =    0;   // Legacy command value for smoke = off
const byte SMOKE_ON                  =    3;   // Legacy command value for smoke = high
const byte STARTUP_SLOW              =  251;   // Legacy command value for slow loco startup sequence
const byte STARTUP_FAST              =  252;   // Legacy command value for immediate loco startup
bool smokeOn                         = false;  // Defined during registration; turn on loco smoke when starting up?
bool slowStartup                     = false;  // Defined during registration; fast or slow startup sequence when starting locos?
bool registrationComplete            = false;  // registrationComplete is used during Registration mode...

// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// *** ROUTE REFERENCE TABLES...


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
// Used only in A-MAS and A-LEG as of 01/21/17; possibly also A-OCC?
// Can also add constant fields that indicate incoming speed L/M/H: EB from W, and WB from E, if useful for decelleration etc.
// 1/18/17: A-LEG doesn't care about block "reservations," but needs the table for reference data - specifically for a given block
// to look up the block length, block speed, and possibly parameters such as "Tunnel" -- when populating Delayed Action records and
// to calculate destination block slow-down parameters.
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

// Create a 64-byte array to hold the status of every sensor 1..64 (sensor 1 is at element 0.)  0 = clear, 1 = occupied.
byte sensorStatus[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// TRAIN REFERENCE TABLE Rev: 01-15-17.
// BE SURE TO ALSO UPDATE IN OTHER MODULES THAT USE THIS STRUCTURE OR HARD-CODED DATA IF CHANGED.
// Used only in A-MAS and A-LEG as of 01/21/17.
// VERSION 01/15/17 added short and long whistle/horn parameters needed for Delayed Action table
// VERSION 09/24/16 added train length to train ref table.  Prev 07/15/16

struct trainReferenceStruct {
  char alphaDesc[9];             // Alphanumberic description only used by A-OCC when prompting operator for train IDs during registration
  char engOrTrain;               // E or T for Legacy commands
  unsigned int legacyID;         // Up to 4-digit used by Legacy as engine or train number
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

// TRAIN REFERENCE DATA Rev: 01-15-17.
// This is a READ-ONLY table, total size around 328 bytes.  Could be stored in FRAM.
// Note that train number 1..8 corresponds with element 0..7.  
// This is also our CROSS REFERENCE where we lookup the Legacy ID (and if engine or train), versus the "1 thru 8" programming train number.
// IMPORTANT: Easier just to hard-code here vs. moving into FRAM because easier to make changes.
// HOWEVER, this is used by both A-LEG *and* A-MAS so be sure to check "version number" and change in both as needed.
trainReferenceStruct trainReference[MAX_TRAINS] = {
  {"WP 803A ",'T', 803, 'D', 'F', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1, 50, 135, 3, 80, 264, 5, 110, 438, 7,},
  {"BIG BOY ",'E',  14, 'S', 'F', 122, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1, 50, 135, 3, 70, 215, 5, 100, 374, 7,},
  {"SF 3751 ",'E',   0, 'S', 'P', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,},
  {"MOHAWK  ",'E',   0, 'S', 'F', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,},
  {"SF 4-4-2",'E',   0, 'S', 'F', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,},
  {"SF GP-7 ",'E',   0, 'D', 'f', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,},
  {"SP RF-16",'T',   0, 'D', 'P', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,},
  {"SF PA   ",'T',   0, 'D', 'P', 100, 100, 350, 1, 250, 4, 1000, 15, 20, 0, 1,  0,   0, 3,  0,   0, 5,   0,   0, 7,}
};

// TRAIN DECELERATION STRUCTURE Rev: 1/17/17.
struct trainDecelerationStruct {
  byte trainNum;                  // 1..8 matches Train Reference; not same as Legacy ID which could be a Train or Engine of any value up to 4 digits
  byte fromSpeed;                 // The Legacy speed 1..199 that the train enters the siding; corresponds to Slow, Med, or High speed
  unsigned int mmPerSecond;       // Speed in mm/second that the train travels at "fromSpeed."
  byte toSpeed;                   // The Legacy speed 1..199 that is our target "crawl" speed.  Always 20 as of 1/17/17.
  byte momentum;                  // The Legacy momentum setting 1..8 at which the stopping distance is calculated
  unsigned int mmDistance;        // Distance required to slow from "fromSpeed" to "toSpeed" at "momentum", not including latency.
};

// TRAIN DECELERATION DATA Rev: 01-17-17.
// This is a READ-ONLY table, total size around 640 bytes.  Could be stored in FRAM 3.
// Update this data when new trains are added, or when train data doesn't correspond to actual observation.
const byte DECELERATION_RECS = 19;  // IMPORTANT: Change this value as data records are added/removed
trainDecelerationStruct trainDeceleration[DECELERATION_RECS] = {
  {1,  50, 135, 20, 8, 3353,},   // Train number, entry speed, mm/sec at entry speed, crawl speed, momentum used for distance calc, distance to stop
  {1,  50, 135, 20, 7, 1715,},   // Train 1 is WP 803 F-unit
  {1,  50, 135, 20, 6,  914,},
  {1,  50, 135, 20, 5,  483,},
  {1,  80, 264, 20, 7, 5486,},
  {1,  80, 264, 20, 6, 2794,},
  {1,  80, 264, 20, 5, 1499,},
  {1, 110, 438, 20, 5, 3251,},
  {1, 110, 438, 20, 4, 1753,},
  {2,  50, 135, 20, 8, 3404,},   // Train 2 is UP Big Boy
  {2,  50, 135, 20, 7, 1740,},
  {2,  50, 135, 20, 6,  914,},
  {2,  50, 135, 20, 5,  489,},
  {2,  70, 215, 20, 7, 4038,},
  {2,  70, 215, 20, 6, 2057,},
  {2,  70, 215, 20, 5, 1067,},
  {2, 100, 374, 20, 6, 5029,},
  {2, 100, 374, 20, 5, 2253,},
  {2, 100, 374, 20, 4, 1397,}
};

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

// *** DELAYED ACTION TABLE
// Define the structure that will hold individual Delayed Action table records.  Will be stored in FRAM2.
// IMPORTANT REGARDING parm2.  I can't think of when we would use parm2, unless we need it as a flag or something for the
// special case when we trip a destination entry sensor and must convert a Sensor record to a (delayed) Timer record???

struct delayedAction {
  char status;                    // Sensor-type, Time-type, or Expired
  byte sensorNum;                  // 0 if n/a (Time-type), or 1..52 (or n/a if Time type)
  byte sensorTripType;            // 0=Clear, 1=Trip (or n/a if Time type)
  unsigned long timeRipe;         // Time in millis() to execute this record (or n/a if Sensor type)
  char deviceType;                // Engine, Train, Accessory, sWitch, or Route (usually E or T, sometimes Accessory)
  byte deviceNum;              // Engine or train number, or accessory number, etc.
  char cmdType;               // E, A, M, S, B, D, F, C, T,  or Y
  byte parm1;           // i.e. command parm, or 0 or 1 for Acc'y off/on
  byte parm2;           // Possibly never used.  Maybe used for horn length, intensity?
};
delayedAction actionElement = { 'E',0,0,0,'E',0,' ',0,0 };  // Use this to hold individual elements when retrieved

// We use a variable totalDelayedActionRecs (in FRAM2) so that we can keep track of the maximum number of records used in the Delayed Action Table
// at any given time.  It will increase as needed, to the max number of Delayed Action records needed at any one time in a session.
// An 8MB FRAM can hold over 600 Delayed Action table records, so this will never be an issue unless we use FRAM2 for more than just this one table.
// We increment totalDelayedActionRecs any time we must insert a record that is higher than this number, but often we will use an earlier "expired"
// record.  Without this variable, we would need to scan the ENTIRE Delayed Action Table every time we wanted to search for "ripe" records.
// But with this variable, we only need to search as far as the max number of records needed so far, possibly a much shorter/faster search.
// This is not NECESSARY, but it is easy to implement and will speed things up somewhat.  And no need to pre-decide on the table size; can expand.
// It's important to reset this to zero each time we re-initialize the Delayed Access table, such as when we begin Auto or Park mode.  In fact, no
// other initialization of the Delayed Action table is necessary, regardless of what old values may be there -- because as long as we reset the
// totalDelayedActionRecs variable, we will never read more than that many records -- zero to begin with.
// Thus there is no "initialize delayed action table" function needed, just reset this variable to zero (except to populate with test data if desired.)
unsigned int totalDelayedActionRecs = 0;    // This is first "new" record in the Delayed Action table.  Equals the number of occupied records so far.

// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(115200);                // RS485 can run at 115200 baud
  Serial3.begin(9600);                  // Legacy serial interface
  Wire.begin();                         // Start I2C for Centipede shift register
  shiftRegister.initialize();           // Set all registers to default
  initializeShiftRegisterPins();        // This ensures NO relays (thus turnout solenoids) are turned on (which would burn out solenoids)
  initializePinIO();                    // Initialize all of the I/O pins
  initializeLCDDisplay();               // Initialize the Digole 20 x 04 LCD display
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);
  initializeFRAM1AndGetControlBlock();  // Check FRAM1 version, and get control block
  sprintf(lcdString, "FRAM1 Rev. %02i/%02i/%02i", FRAM1GotVersion[0], FRAM1GotVersion[1], FRAM1GotVersion[2]);  // FRAM1 version no. on LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);
  initializeFRAM2();
  sprintf(lcdString, "FRAM2 OK.");
  sendToLCD(lcdString);
  Serial.println(lcdString);
  for (byte i = 0; i < MAX_TRAINS; i++) {
    trainProgressInit(i + 1);  // First train is Train #1 (not zero) because functions accept actual train numbers, not zero-offset numbers.
  }

  #ifdef TEST_DATA
    populateDelayedActionTable();       // For test mode only, put some test data in the Delayed Action table.
  #endif

}

// ***************************************************************************************
// **************************************  L O O P  **************************************
// ***************************************************************************************

void loop() {

  checkIfHaltPinPulledLow();  // If someone has pulled the Halt pin low, release relays and just stop

  checkIfPowerMasterOnOffPressed();   // Check the four control panel "PowerMaster" on/off switches to turn power on or off

  // We need to watch for the following RS485 messages:
  //   In ANY mode:
  //     From A-MAS to ALL broadcasting change of mode and state (i.e. Register Mode, Running)
  //     From A-SNS to A-MAS reporting a sensor status change.
  //   In REGISTER mode, RUNNING state:  IMPORTANT: Sensor status MUST NOT change during Registration, all trains must be stopped!
  //     From A-OCC to A-MAS certain replies from the operator via a/n display.  i.e. SMOKE Y/N, STARTUP FAST/SLOW.
  //     From A-OCC to A-MAS replies with operator registration data.  A-LEG should establish a fresh Train Progress record and startup the loco.
  //   In AUTO or PARK mode, RUNNING or STOPPING state:
  //     From A-MAS to A-LEG assigning a new route to a train (will not occur if state = STOPPING)
  //     From A-MAS to ALL, changing state to STOPPING -- probably nothing special to do until another mode started.
  //     From A-MAS to ALL, changing state to STOPPED -- continue to maintain Train Progress in case we start Auto again???
  //       NOTE: A-LEG does not care about block reservations, so nothing to do there -- but we do use the table for reference to block data.

  // IN ANY MODE, CHECK INCOMING RS485 MESSAGES...
  if (RS485GetMessage(RS485MsgIncoming)) {   // WE HAVE A NEW RS485 MESSAGE! (may or may not be relevant to us)

    // Need to reset the state and mode changed flags so we don't execute special code more than once...
    stateChanged = false;
    modeChanged = false;

    // ***********************************************************************************
    // ********** CHECK FOR MODE/STATE-CHANGE MESSAGE AND UPDATE AS APPROPRIATE **********
    // ***********************************************************************************
    // We will have special code required for mode changes below, associated with each mode's code block.
    // As of 2/28/17, we only have special code for Register mode (initialize Train Progress and Delayed Action tables.)
    if (RS485fromMAStoALL_ModeMessage()) {  // If we just received a mode/state change message from A-MAS
      RS485UpdateModeState(&modeOld, &modeCurrent, &modeChanged, &stateOld, &stateCurrent, &stateChanged);
    }

    // ****************************************************************
    // ********** CODE ASSOCIATED WITH MANUAL / P.O.V. MODES **********
    // ****************************************************************

    // CHECK FOR SENSOR-CHANGE MESSAGE AND HANDLE FOR MANUAL / P.O.V. MODES...
    // If we are in Manual or POV mode, regardless of state, simply update the sensor status
    if ((modeCurrent == MODE_MANUAL) || (modeCurrent == MODE_POV)) {
      if (RS485fromSNStoMAS_SensorMessage()) {
        byte sensorNum = RS485MsgIncoming[4];
        byte sensorTripType = RS485MsgIncoming[5];   // 0 = cleared, 1 = tripped
        sensorStatus[sensorNum - 1] = sensorTripType;   // Will be 0 or 1, for off or on
      }
    }

    // ************************************************************
    // ********** CODE ASSOCIATED WITH REGISTRATION MODE **********
    // ************************************************************

    // If the MODE changed, and we are now (newly) in REGISTRATION, RUNNING, then we need to (re)init the train data.
    if (modeChanged || stateChanged) {    // Either mode or state changed, so do some special handling
      if ((modeCurrent == MODE_REGISTER) && (stateCurrent == STATE_RUNNING)) {
        // When we begin Registration mode, we will want to clear out the Train Progress table completely.
        for (byte i = 0; i < MAX_TRAINS; i++) {
          trainProgressInit(i + 1);  // First train is Train #1 (not zero) because functions accept actual train numbers, not zero-offset numbers.
        }
        // Also, reset the Delayed Action table (and we will never see this code if we are running in Test mode.)
        // We don't need to overwrite all of the FRAM2 records, we simply need to set totalDelayedActionRecs back to zero.
        // I don't think we need to actually write any 'Expired' records, because we're saying there are zero records.  So we won't search even
        // the first record, and will start adding at the first record.
        totalDelayedActionRecs = 0;    // Equals the number of occupied records so far.
        // Set "registration complete" flag to false when we begin registration.
        registrationComplete = false;
      }
    }

    // CHECK FOR SENSOR-CHANGE MESSAGE IN REGISTRATION MODE IS A FATAL ERROR...
    // We'll trigger a Halt because sensors should never change during registration.  Not because A-OCC shouldn't send them (it should),
    // but because no train should be moving during registration!  So not a programming error but an operator error.
    if (modeCurrent == MODE_REGISTER) {
      if (RS485fromSNStoMAS_SensorMessage()) {
        sprintf(lcdString, "%.20s", "SNS CHG DURING REG!");
        sendToLCD(lcdString);
        Serial.print(lcdString);
        endWithFlashingLED(3);
      }
    }

    // CHECK FOR SMOKE ON/OFF COMMAND FROM A-MAS.
    // This will only occur when we are in Registration mode.
    if (RS485fromMAStoLEG_SmokeMessage()) {
      smokeOn = RS485GetSmokeOn();  // Returns smokeOn = true or false
    }

    // CHECK FOR STARTUP FAST/SLOW COMMAND FROM A-MAS.
    // This will only occur when we are in Registration mode.
    if (RS485fromMAStoLEG_FastOrSlowStartupMessage()) {
      slowStartup = RS485GetSlowStartup();  //  Returns slowStartup = true or false
    }

    // If the RS485 message is a "New Train Registered" message from A-OCC (via operator), then register and startup the train.
    // This should only occur when we are in Registration mode, so no need to check.
    if (RS485fromOCCtoMAS_RegistrationMessage()) {  // This confirms it's an OCC "train registration" message, though could be real or "done" type.
      // Read records and register trains until we get a 'T' record with RS485MsgIncoming[7] == 'Y' indicating last record.
      // Note that the last record contains NO TRAIN DATA so don't try to read it.
      if (registrationComplete == false) {
        // Note that when the operator clicks "DONE" that all trains have been registered (or when A-OCC recognizes it because they have indicated a block
        // number for every possible train), A-OCC will send a record with *only* the "last record" field set to Y; i.e. no train data.
        if (RS485fromOCCtoMAS_TrainRecord()) {   // Is it a new train data record, versus an all-done record?
          // PROCESS INCOMING DATA FOR A SINGLE NEWLY-REGISTERED TRAIN...
          // Great!  We have a registered train record.  Get the data we need from the incoming RS485 buffer...
          // Note that A-OCC will not send us data for STATIC trains, only up to the 8 "real" trains.
          byte tTrain = TRAIN_ID_NULL;
          byte tBlock = 0;
          char tDir = ' ';
          byte tEntSns = 0;
          byte tExtSns = 0;
          RS485fromOCCtoMAS_ExtractData(&tTrain, &tBlock, &tDir, &tEntSns, &tExtSns);
          // Now, create an entry in the newly-initialized Train Progress table for this newly-registered train...
          trainProgressEnqueue(tTrain, tBlock, tEntSns, tExtSns);
          // TEST CODE: DISPLAY RESULTS OF WHAT WE HAVE SO FAR...AS EACH TRAIN IS REGISTERED...
          Serial.println("NEW TRAIN REGISTERED!");
          Serial.print("Train number: "); Serial.println(tTrain);
          Serial.print("Block number: "); Serial.println(tBlock);
          Serial.print("Direction   : "); Serial.println(tDir);
          Serial.print("Entry sensor: "); Serial.println(tEntSns);
          Serial.print("Exit sensor : "); Serial.println(tExtSns);

          // Now start up the train!  Do this by populating the Delayed Action table.
          // Start by adding a record for fast or slow startup.  Then set smoke Y/N, abs speed zero, and direction forward.
          // Startup is a 'B'asic command
          actionElement.status = 'T';           // Time-type (versus Sensor-type or Expired record)
          actionElement.sensorNum = 0;           // 0 if n/a (Time-type), or 1..52
          actionElement.sensorTripType = 0;     // n/a since Timer record not Sensor record
          actionElement.timeRipe = millis();    // time in millis() to execute this record = asap
          actionElement.deviceType = trainReference[tTrain - 1].engOrTrain;  // Engine, Train, Accessory, sWitch, or Route (usually E or T, sometimes Accessory)
          actionElement.deviceNum = trainReference[tTrain - 1].legacyID;  // Engine or train number, or accessory number, etc.
          actionElement.cmdType = 'B';      // E, A, M, S, B, D, F, C, T,  or Y
          if (slowStartup == true) {
            actionElement.parm1 = STARTUP_SLOW;       // 251 = slow startup
          } else {  // Fast startup
            actionElement.parm1 = STARTUP_FAST;       // 252 = fast startup
          }
          actionElement.parm2 = 0;                    // Possibly never used.  Maybe used for horn length, intensity?
          writeActionElement();                       // Add this record to the Delayed Action table

          // Smoke is a 'C'ontrol command
          actionElement.timeRipe = millis() + 15000;  // time in millis() to execute this record
          actionElement.cmdType = 'C';                // E, A, M, S, B, D, F, C, T,  or Y
          if (smokeOn == true) {
            actionElement.parm1 = SMOKE_ON;           // 3 = smoke on high
          } else {
            actionElement.parm1 = SMOKE_OFF;          // 0 = smoke system off
          }
          writeActionElement();                 // Add this record to the Delayed Action table

          // 'S'top immed. preferred over 'A'bse speed zero, as it overrides any existing momentum if loco is moving
          actionElement.timeRipe = millis() + 15100;    // time in millis() to execute this record
          actionElement.cmdType = 'S';      // E, A, M, S, B, D, F, C, T,  or Y
          actionElement.parm1 = 0;              // n/a
          writeActionElement();                 // Add this record to the Delayed Action table

          // Forward direction just for good measure, is a 'B'asic command
          actionElement.timeRipe = millis() + 15000;    // time in millis() to execute this record
          actionElement.cmdType = 'B';      // E, A, M, S, B, D, F, C, T,  or Y
          actionElement.parm1 = 0;              // Basic cmd 0 = forward
          writeActionElement();                 // Add this record to the Delayed Action table

        } else {   // This is not a train data record; it's an "all done" record
          registrationComplete = true;
          Serial.println("Registration complete.");
        }
      }     // End of "if registration complete is false" loop
    }       // It's a Train Registration message from A-OCC

    // ************************************************************
    // ********** CODE ASSOCIATED WITH AUTO / PARK MODES **********
    // ************************************************************

    // CHECK FOR SENSOR-CHANGE MESSAGE WHEN RUNNING IN AUTO or PARK MODES...
    // 1. Update the SENSOR STATUS array
    // 2. Update the TRAIN PROGRESS and DELAYED ACTION tables, as appropriate:
    //      BLOCK ENTRY SENSOR TRIP and *IS* the DESTINATION: Add DELAYED ACTION RECORDS with commands to bring train to a stop,
    //        including delayed Timer records for slowing down, and Sensor records to stop train immediately upon tripping destination exit sensor.
    //        Get block length from BLOCK RESERVATION table, and slow-down data from TRAIN REFERENCE table.
    //      BLOCK ENTRY SENSOR TRIP and *NOT* the DESTAINATION: Nothing specific to do.
    //      BLOCK ENTRY SENSOR CLEAR: Nothing specific to do -- the train is just moving along.
    //      BLOCK EXIT SENSOR TRIP and *IS* the DESTINATION: Nothing specific to do, but when we scan Delayed Action there will be a command to stop train immediately.
    //      BLOCK EXIT SENSOR TRIP and *NOT* the DESTINATION: Use BLOCK RESERVATION to retrieve the NEXT block's speed i.e. Medium, and TRAIN REFERENCE to retrieve
    //        this train's Legacy speed i.e. 45, and add "timer immed" momentum & abs. speed records to DELAYED ACTION to adjust the train's speed.
    //      BLOCK EXIT SENSOR CLEAR: REMOVE THE BLOCK FROM THE TRAIN PROGRESS TABLE and decrement the length.
    //      ANY TRIP OR CLEAR: Check the Sensor Action Reference table and see if we need to add any records to Delayed Action, such as Sound off, Crossbuck on, etc.
    //    I don't any of the above requires updates to the Block Reservation table because we use it as read-only for parms, not for reservations.
    // 3. Scan the entire DELAYED ACTION table and execute all SENSOR records for THIS SENSOR and trip type (trip/clear.)  For example, if this
    //      is a DESTINATION EXIT SENSOR TRIP, then there will be a record to Stop Immed upon tripping this sensor.  But this could also
    //      include things like lowering a crossbuck, turning off sound, etc.
    // 4. OPTIONAL: Scan the entire Delayed Action table and execute all Timer records that are ripe (could we have just created some that are "execute immediate"?)
    //    However, we would otherwise see this farther down in the loop so maybe not needed here.


/*


    if ((modeCurrent == MODE_AUTO) || (modeCurrent == MODE_PARK)) {
      if (RS485fromSNStoMAS_SensorMessage()) {
        byte sensorNum = RS485MsgIncoming[4];
        byte sensorTripType = RS485MsgIncoming[5];   // 0 = cleared, 1 = tripped
        // Update the sensor status array.
        sensorStatus[sensorNum - 1] = sensorTripType;   // Will be 0 or 1, for clear or tripped
        byte sensorTrain = 0;  // We will populate this with the "real" train number (1..n) that tripped the sensor, based on Train Progress table.
        // Scan the Train Progress table to find the sensor, and that will identify the Train and the Block numbers.
        // There can only be EXACTLY ONE reference to this sensor in the Train Progress table, so we can stop as soon as we find it.
        // If a sensor that has just been tripped or cleared is not found in any active Train Progress record, then we have a fatal error.
        bool sensorFound = false;
        do {
          for (byte tTrain = 0; tTrain < MAX_TRAINS; tTrain++) {   // For each set of train records in Train Progress table
            // tTrain is the "real" train number - 1.  I.e. tTrain starts a zero, real train numbers start at 1.
            if (trainProgressPointer[tTrain].len > 0) {   // If this train has been defined in Registration; else ignore.
              for (byte tBlock = 0; tBlock < trainProgressPointer[tTrain].len; tBlock++) {   // For each block in this train's route
                // tBlock counts the total number of blocks in Train Progress for this train, but is *not* the block record number
                byte tPointer = (trainProgressPointer[tTrain].head + tBlock) % MAX_BLOCKS_PER_TRAIN;
                // tPointer is the actual array index in Train Progress that we are concerned with in this iteration.
                // tPointer starts at the head/destination then increments (MOD total_records) until it arrives at the tail (typically where the train will currently be.)
                if (trainProgress[tTrain][tPointer].entrySensor == sensorNum) {   // Our sensor is an ENTRY SENSOR!
                  sensorTrain = tTrain + 1;   // This will be the "real" train number 1..n
                  if (sensorTripType == 1) {  // ENTRY SENSOR of this block was TRIPPED.
                    if (tBlock == 0) {       // ENTRY SENSOR of this block was TRIPPED and this *IS* the DESTINATION block.
                      // Populate Delayed Action with commands to begin stopping the train...including Timer records to begin slowing, and Sensor records for stop immed.
                      // This will include a Delayed Action record for Stop Immed when the destination block's exit sensor is tripped.
                      // Get block length from BLOCK RESERVATION table, slow-down data from TRAIN REFERENCE table, and deceleration data from TRAIN DECELERATION table.
                      sprintf(lcdString, "Prep stop loco %2d", sensorTrain);
                      sendToLCD(lcdString);
                      Serial.print(lcdString);
                      // For the initial version 3/1/17, let's keep it simple without a bunch of calculations, and just set momentum to medium, wait a few seconds,
                      // then set absolute speed to Crawl.  Also turn on the bell.  Also add a 'S'ensor 'T'rip record for Stop Immed on tripping dest. exit sensor.
                      // IN THE FUTURE, we need to add a lot of code to calculate delay and slow-down parameters based on deceleration data
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis();  // Execute asap
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'B';  // Basic cmd 245 = bell on
                      actionElement.parm1 = 245;
                      writeActionElement();  // Add this record to the Delayed Action table
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis();  // Execute asap
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'M';  // Momentum
                      actionElement.parm1 = MOMENTUM_CHANGING;  //  i.e. around 3 or 4
                      writeActionElement();  // Add this record to the Delayed Action table
                      byte tLegacySpeed = trainReference[sensorTrain - 1].crawlSpeed;
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis() + 100;  // Execute asap
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'A';  // Absolute speed
                      actionElement.parm1 = tLegacySpeed;  //  i.e. around 50 or so (0..199)
                      writeActionElement();  // Add this record to the Delayed Action table
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis() + 12000;  // Execute in 12 seconds
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'B';  // Basic cmd 244 = bell off
                      actionElement.parm1 = 244;
                      writeActionElement();  // Add this record to the Delayed Action table
                      actionElement.status = 'S';         // Sensor record
                      actionElement.sensorNum = trainProgress[tTrain][tPointer].exitSensor;
                      actionElement.sensorTripType = 1;   // Sensor Trip
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'S';  // Stop immediate
                    } else {                  // ENTRY SENSOR of this block was TRIPPED and this is *NOT* the destination.
                      // Nothing specific to do here.
                    }
                  } else {                    // ENTRY SENSOR of this block was CLEARED (whether it was the destination block or not)
                    // Just means train is progressing along; no Train Progress table update
                    // Nothing specific to do here, whether this is the destination or not.
                  }
                  sensorFound = true;
                  // Could maybe put a break statement here???
                } else if (trainProgress[tTrain][tPointer].exitSensor == sensorNum) {   // Our sensor is an EXIT SENSOR!
                  sensorTrain = tTrain + 1;   // This will be the "real" train number 1..n
                  if (sensorTripType == 1) {  // EXIT SENSOR of this block was  TRIPPED
                    if (tBlock == 0) {       // EXIT SENSOR of this block was TRIPPED and this *IS* the DESTINATION block.
                      // Nothing specific to do here.
                    } else {                  // EXIT SENSOR of this block was TRIPEED and this is *NOT* the destination.
                      // Retrieve the NEXT block number from the TRAIN PROGRESS table i.e. 17
                      // Retrieve the NEXT block's speed from the BLOCK RESERVATION table i.e. Medium
                      // Retrieve this train's Legacy speed from the TRAIN REFERENCE table i.e. 45
                      // Create new 'Timer' immediate records with momentum and target speed in the DELAYED ACTION table
                      // tTrain is the "real train number - 1".  I.e. tTrain starts a zero, real train numbers start at 1.
                      // tBlock counts the total number of blocks in Train Progress for this train, but is *not* the block record number
                      // tPointer is the actual array index in Train Progress that we are concerned with in this iteration.
                      // tPointer starts at the head/destination then increments (MOD total_recs) until it arrives at the tail (typically where the train will currently be.)
                      sprintf(lcdString, "Adj loco %2d speed.", sensorTrain);
                      sendToLCD(lcdString);
                      Serial.print(lcdString);
                      // Since tPointer is the index in Train Progress to the current block's record (0..max blocks - 1), we need tPointer - 1, but accounting for circular table

// 3/ 10/17: NEED TO FIX the following: trainProgress requires two array indexes, not one as below.  Train number and also the record number to look up the block number of the next block.

                      if (tPointer = 0) {
                        byte tNextBlock = trainProgress[MAX_BLOCKS_PER_TRAIN - 1].blockNum;
                      } else {
                        byte tNextBlock = trainProgress[tPointer - 1].blockNum;
                      }
                      // tNextBlock is now the "real" block number of the next block
                      char tBlockSpeed = blockReservation[tNextBlock - 1].blockSpeed;   // This will be 'L', 'M', or 'H'
                      if (tBlockSpeed == 'L') {
                        byte tLegacySpeed = trainReference[sensorTrain - 1].lowSpeed;
                      } else if (tBlockSpeed == 'M') {
                        byte tLegacySpeed = trainReference[sensorTrain - 1].mediumSpeed;
                      } else if (tBlockSpeed == 'H') {
                        byte tLegacySpeed = trainReference[sensorTrain - 1].highSpeed;
                      } else {  // Fatal error
                        sprintf(lcdString, "ERR loco speed ' %c '", tBlockSpeed);
                        sendToLCD(lcdString);
                        Serial.print(lcdString);
                        endWithFlashingLED(3);
                      }
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis();  // Execute asap
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'M';  // Momentum
                      actionElement.parm = MOMENTUM_CHANGING;  //  i.e. around 3 or 4
                      writeActionElement();  // Add this record to the Delayed Action table
                      actionElement.status = 'T';         // Timer record
                      actionElement.timeRipe = millis() + 100;  // Execute asap
                      actionElement.deviceType = trainReference[sensorTrain - 1].engOrTrain;
                      actionElement.deviceNum = trainReference[sensorTrain - 1].legacyID;
                      actionElement.cmdType = 'A';  // Absolute speed
                      actionElement.parm = tLegacySpeed;  //  i.e. around 50 or so (0..199)
                      writeActionElement();  // Add this record to the Delayed Action table
                      // I suppose we could try to process any "ripe" Delayed Action timer records now (such as the two we just created) but probably best to
                      // let this happen as code continues throught the main module loop.
                    }
                  } else {                    // EXIT SENSOR of this block was CLEARED
                    // We should remove this block from the Train Progress table and decrement the length.
                    // No need to check if this is the destination block, because we will never clear the exit sensor of a destination - it's impossible.
                    // We assume that the entry sensor must be clear or fatal error, so check just to be certain.  Of course it's also possible that the train
                    // is somehow in the middle of the block and both end sensors are clear - this would be a real problem, but should be impossible for a block
                    // exit sensor to clear if the train wasn't on the way out.
                    // Incidentally, this had also better be the tail block!
                    // So for now, just make sure that this is the tail block, and its entry sensor is clear, before we proceed.
                    // tTrain is the "real train number - 1".  I.e. tTrain starts a zero, real train numbers start at 1.
                    // tBlock counts the total number of blocks in Train Progress for this train, but is *not* the block record number
                    // tPointer is the actual array index in Train Progress that we are concerned with in this iteration.
                    // tPointer starts at the head/destination then increments (MOD total_records) until it arrives at the tail (typically where the train will currently be.)
                    sprintf(lcdString, "Loco %2d clr blk %2d", sensorTrain, trainProgress[tTrain][tPointer].blockNum);
                    sendToLCD(lcdString);
                    Serial.print(lcdString);
                    if (tBlock != (trainProgressPointer[tTrain].len - 1)) {  // We should never clear an exit sensor unless it's the tail.
                      sprintf(lcdString, "%.20s", "EX SNS CLR NOT TAIL!");
                      sendToLCD(lcdString);
                      Serial.print(lcdString);
                      endWithFlashingLED(3);
                    }
                    // The sensor number for the entry sensor is trainProgress[tTrain][tPointer].entrySensor
                    // The sensor status will be in sensorStatus[sensorNum - 1], and will be zero or one.
                    if (sensorStatus[trainProgress[tTrain][tPointer].entrySensor - 1] == 1) {    // Yikes, the entry sensor is set!  Fatal error!
                      sprintf(lcdString, "ERR entry sns %2d set", trainProgress[tTrain][tPointer].entrySensor);
                      sendToLCD(lcdString);
                      Serial.print(lcdString);
                      endWithFlashingLED(3);
                    }
                    // Just to be sure this isn't the destination block, we'll do one more quick fatal error check...
                    if (trainProgressPointer[tTrain].len < 2) {
                      // If the length of the Train Progress records is 0 or 1, this is a fatal condition that should never occur
                      sprintf(lcdString, "%.20s", "ERR CLR DST EXIT BLK");
                      sendToLCD(lcdString);
                      Serial.print(lcdString);
                      endWithFlashingLED(3);
                    }
                    // No fatal error conditions that we can find, so go ahead and remove this record from Train Progress.
                    // To do so, all we need to do is decrement the pointer length value by 1.
                    trainProgressPointer[tTrain].len--;
                  }
                  sensorFound = true;
                  // Could maybe put a break statement here???
                }
              }
            }
          }
          // We might want to put a fatal error check here if we have checked the entire table, or the error would be an infinite loop here
        } while (sensorFound == false);
        // We found the sensor sensorNum in the Train Progress table for train sensorTrain.
        // No matter what we did above, we ALSO want to potentially add "generic" records to Delayed Action for things like sound off, acc'y on, etc.




      }
    }      // End of handling the RS485 sensor changed message when in Auto/Park mode

    // CHECK FOR "NEW ROUTE ASSIGNED" MESSAGE FROM A-MAS IN AUTO or PARK MODE
    // This should only occur when we are in Auto or Park mode, so no need to check.
    if (RS485fromMAStoLEG_RouteMessage()) {

      // CODE HERE TO HANDLE NEW ROUTE SETUP.
      // This could be the initial route when a train has just a single Train Progress record (either newly registered, or having
      // completed a previous route,) or it could be a "continuation" route, in which case the train will be incoming, at some point
      // between the penultimate exit sensor and the destination entry sensor.

      // In Auto (and Park) mode, when we first create a Train Progress record for a train, maybe we should confirm that the sensors
      //   along the route, especially in the origin block, are set/cleared at that moment according to our expectations - else fatal error.



      // JUST FOR FUN, let's confirm that each block of the NEW route (not including previous route) has all sensors clear. Otherwise it means
      // that there is a train in our path, which should never happen if A-MAS is working properly, but why not double-check here?
      // trainProgressPointer[t - 1].head = [0..max_blocks_per_train - 1] always points to the destination block.
      // trainProgressPointer[t - 1].length = [1..total_blocks_reserved_for_this_train] indicates the number of blocks currently reserved.
      // We can do this as we add Route Reference records to the Train Progress table.  Each new block must be clear.
      // Note that A-MAS assigns new routes at two possible moments:
      // 1.  When a train is moving, AFTER it has tripped the exit sensor of the block immediately preceeding the destination block, but has
      //     NOT YET TRIPPED the destination entry sensor.  This could be a VERY SHORT amount of time, so our logic must be fast.
      // 2.  When the train is sitting in the one-and-only block, and is stopped.  Such as when first starting Auto mode, or if a train had
      //     to be stopped because a subsequent route could not be found when the train was still moving.

// 2/28/17: Still working on this...

      if ((sensorStatus[trainProgress[t - 1][x].entrySensor] != 0) || (sensorStatus[trainProgress[t - 1][x].exitSensor] != 0)) {
        sprintf(lcdString, "%.20s", "BLOCK OCCUPIED ERR!");
        sendToLCD(lcdString);
        Serial.print(lcdString);
        endWithFlashingLED(3);
      }




    }

*/



  }  // End of "if we have an RS485 incoming message"

  // *************************************************************
  // *************************************************************
  // ********** END OF HANDLING INCOMING RS485 MESSAGES **********
  // *************************************************************
  // *************************************************************

  // *****************************************************************************************
  // *****************************************************************************************
  // ********** LOGIC TO SCAN THE DELAYED-ACTION TABLE AND TAKE ACTION AS INDICATED **********
  // *****************************************************************************************
  // *****************************************************************************************
  
  // Scan the Delayed Action table starting at the beginning for records that qualify for action.
  // As soon as we find any qualifying record, we'll flag it as Expired, exit the search loop, and proceed to the "execute a command" code.
  // If this is an EXPIRED record, then just move on to the next record.
  // If this is an ACTIVE, TIMED record and our time has not yet come up, then skip the record and move on
  // If this is an ACTIVE, TIMED record that is RIPE, the UPDATE the record as Expired, and EXECUTE the command (train speed, etc.)
  // When we get a "sensor status change" message from A-SNS, we'll scan the Delayed Action table to see if there are any action(s) that we
  //   need to take based on that.  Note that we could have MORE THAN ONE record for a given sensor change, so we need to do this in a loop
  //   that scans the table until we make it to the end without any "Sensor" record hits.
  // Note that we exit the loop as soon as we find a ripe record, always starting at the beginning.






  byte actionRecFound = false;     // Set this to true if we find a record in the following table scan

  // IMPORANT: We eventually need to change the following for.. loop to not scan the entire table, but only the highest active record.
  // We defined a variable for this above, highestValidDelayedActionRec = 0, be we need to establish a value as we populate the table
  // and as we add records (if they must be added to the top of the table rather than an expired slot.)



  // IMPORTANT 1/20/17: The following needs to be broken into two separate functions:
  // getActionTableTimerElement, which searches and returns a ripe record if one is found.  If it's the last record, maybe also decrements last-record variable.
  // getActionTableSensorElement, which searches and returns a record that matches the sensor number and type (trip/clear).
  //   This latter must be in a repeat loop to run until no further records are found, because there could be several commands to run on a given
  //   sensor trip or clear.  And unlike timer records, where we just run from time to time to see what's there, the sensor searches must be run
  //   at the time a sensor change record is received, and all matching records for that sensor+type need to be executed.  There can be zero or more.
  // Special logic needed when we trip the destination entry and destination exit sensors.  Just look at the train progress table to determine if
  // that is the case.
  
  // When we get a new sensor change record from A-SNS, the first thing we need to do is see if it is a DESTINATION SIDING ENTRY SENSOR TRIP
  // for any active train.  When that happens, we need to populate Delayed Action with the whole sequence of "come to a stop" commands including
  // destination exit sensor trip commands.  There may or may not already be Delayed Action record(s) for dest siding, maybe turn on acc'y?
  
  // When we get a new sensor change record from A-SNS that is not a destination siding entry sensor trip, then we must search the Train Progress table
  // to see which train and confirm it's moving as expected i.e. tripping and clearing in order.  Then search Delayed Action for any matches.
  // We may need to allow for CLEARING (but not tripping) slightly out of order, because we could have a longer delay on a relay that caused an earlier
  // release to not show up until after a "later" release just due to the relay time delay.
  
  // When we get a New Route+Train command from A-MAS, we first check the Train Progress table to see if the train has entered the destination siding yet.
  // If so, then we will have already created the set of records to bring the train to a stop.
  // If it has NOT yet entered the destination siding, then we just add continuation records starting with the "old" destination siding.
  // Special case if this is the first time the train has been assigned a route...Train Progress length will be zero so we'll know it, and can handle appropriately.
  
  // When we get a New Route+Train record from A-MAS, we populate train progress with start thru end (potentially extending a route) but the
  // Delayed Action table gets different records depending if the train is moving (i.e. extending a route) to stopped (starting as if new.)
  // If moving, then we start by making an entry for the first block int he route, which is the destingation block for the previous route this train
  // had -- with momentum and speed for this block as a "through" block, plus any other special commands that might be relevant for this block.
  // If stopped, we populate the whole "make announcement and get moving" sequence.
  // But we ALWAYS refrain from any Delayed Action records for the destination block entry or exit sensor as those must be created at the time
  // the destination entry sensor is tripped, because we won't know for sure until then if it's a continuation route, or from a stop.
  
  // When we create a new set of Delayed Action records for a new route, we may eventually need a new table for "Block Actions" that is a lookup for what
  // actions to insert in Delayed Action for a given block, perhaps also other parameters such as loco type etc.  I.e. when entering block 13 eastbound,
  // always turn off sound.  And wen entering 8 eastbound, always turn ON sound.  (Block 21 is a bit tricky with sound because some in and some out of tunnel.)




// 2/27/17: THE FOLLOWING NEEDS TO BE PUT INTO A COUPLE OF FUNCTIONS.
// One to try to retrieve a record, and another to execute.  And maybe each of those needs to go in its own function.


  for (unsigned int delayedActionRec = 0; delayedActionRec < totalDelayedActionRecs; delayedActionRec++) {  // Using offset 0 for ease of calcs

    // FRAM addresses must be UNSIGNED LONG
    unsigned long FRAM2Address = FRAM2_ACTION_START + (delayedActionRec * FRAM2_ACTION_LEN);
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
      // original record.  So it only amounts to updating the record.  We leave actionRecFound false and just move on, and will
      // eventually encounter this record again and process it when the specified millis() time has expired.

    }

    else if ((actionElement.status == 'T') && (actionElement.timeRipe > millis())) {   // Time record, but not yet ripe so move on.
    }

    else if ((actionElement.status == 'T') && (actionElement.timeRipe <= millis())) {  // Time record and it's ripe!
      // Write back the record as "Expired" since we're going to process it now.
      actionElement.status = 'E';    // Set new status of this record to Expired and update the record in FRAM2
      memcpy(b, &actionElement, FRAM2_ACTION_LEN);
      FRAM2.write(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_write, data

      actionRecFound = true;   // Tells the code following this "for" loop to process actionElement.
      break;                      // Bail out of this "for" loop and process the valid element
    }
  }   // End of for...try to find the first applicable record in the Delayed Action table loop

  // If we have a valid record to process in actionElement, then actionRecFound will be "true"

  if (actionRecFound == true) {        // We have a valid record to process!

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
    Serial.print(actionElement.sensorNum);
    Serial.print(", ");
    Serial.print(actionElement.sensorTripType);
    Serial.print(", ");
    Serial.print(actionElement.timeRipe);
    Serial.print(", ");
    Serial.print(actionElement.cmdType);
    Serial.print(", ");
    Serial.print(actionElement.deviceType);
    Serial.print(", ");
    Serial.print(actionElement.deviceNum);
    Serial.print(", ");
    Serial.print(actionElement.parm1);
    Serial.print(", ");
    Serial.println(actionElement.parm2);

    // actionElement.deviceType (1 char) Engine or Train or blank if  n/a
    // actionElement.deviceNum (1 byte) Engine, Train, Acc'y, sWitch, or Route number
    // actionElement.commandType (1 char) [E|A|M|S|B|D|F|C|T|Y|W}R]
    //	 E = Emergency Stop All Devices(FE FF FF) (Note : all other parms in command are moot so ignore them)
    //	 A = Absolute Speed command
    //	 M = Momentum 0..7
    //	 S = Stop Immediate(this engine / train only)
    //   B = Basic 3 - byte Legacy command(always AAAA AAA1 XXXX XXXX)
    //   D = Railsounds Dialogue(9 - byte Legacy extended command)
    //   F = Railsounds Effect(9 - byte Legacy extended command)
    //   C = Effect Control(9 - byte Legacy extended command)
    //   T = TMCC command
    //   Y = Accessory command (parm will be 0=off, 1=on).  On-board relay throw; not Legacy/TMCC.
    //   W = sWitch (UNUSED in any foreseeable version)
    //   R = Route (UNUSED in any foreseeable version)
    // actionElement.parm1 (1 byte) Hex value from Legacy protocol table, if applicable.
    //   N/a for CmdTypes E and S.
    //   For accessories, 0 = Off, 1 = On
    //   For switches, 0 = Normal, 1 = Reverse

    switch (actionElement.cmdType) {
      case 'E':  // Emergency stop all devices
        sprintf(lcdString, "%.20s", "Emergency Stop All");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        legacy11 = 0xFE;
        legacy12 = 0xFF;
        legacy13 = 0xFF;
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'A':   // Set absolute speed
        sprintf(lcdString, "%.20s", "Absolute Speed");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        if (actionElement.deviceType == 'E') {
          legacy11 = 0xF8;
        }
        else {
          legacy11 = 0xF9;
        }
        legacy12 = actionElement.deviceNum * 2;  // Shift left one bit, fill with zero
        legacy13 = actionElement.parm1;
        sprintf(lcdString, "%c %i A.S. %i", actionElement.deviceType, actionElement.deviceNum, actionElement.parm1);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'M':   // Set momentum
        sprintf(lcdString, "%.20s", "Momentum");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        if (actionElement.deviceType == 'E') {
          legacy11 = 0xF8;
        }
        else {
          legacy11 = 0xF9;
        }
        legacy12 = actionElement.deviceNum * 2;  // Shift left one bit, fill with zero
        legacy13 = 0xC8 + actionElement.parm1;
        sprintf(lcdString, "%c %i MOM %i", actionElement.deviceType, actionElement.deviceNum, actionElement.parm1);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'S':    // Stop immediate (this engine or train)
        sprintf(lcdString, "%.20s", "Stop Immediate.");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        if (actionElement.deviceType == 'E') {
          legacy11 = 0xF8;
        }
        else {
          legacy11 = 0xF9;
        }
        legacy12 = actionElement.deviceNum * 2;  // Shift left one bit, fill with zero
        legacy13 = 0xFB;
        sprintf(lcdString, "%c %i STOP IMMED.", actionElement.deviceType, actionElement.deviceNum);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'B':
        // Commands such as FORWARD, REVERSE, HORN, VOL UP, VOL DOWN, BELL ON, BELL OFF, STARUP FAST/SLOW, SHUTDOWN FAST/SLOW.
        // Thus some, like direction, could be sent with LEGACY_REPEATS, but others, like vol up, should only be sent once I think...???
        // Could try using with and without LEGACY_REPEATS on things like horn, and see what happens.  ***********************************************
        sprintf(lcdString, "%.20s", "Basic Legacy 3-Byte");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        if (actionElement.deviceType == 'E') {
          legacy11 = 0xF8;
        }
        else {
          legacy11 = 0xF9;
        }
        legacy12 = (actionElement.deviceNum * 2) + 1;  // Shift left one bit, fill with '1'
        legacy13 = actionElement.parm1;
        sprintf(lcdString, "%c %i BASIC %i", actionElement.deviceType, actionElement.deviceNum, actionElement.parm1);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'D':  // Dialogue (play a track)
      case 'F':  // Effects including VOL UP and VOL DOWN
      case 'C':  // Controls including SMOKE OFF and SMOKE HIGH
        sprintf(lcdString, "%.20s", "Sound D/F/C/ 9-byte");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        if (actionElement.deviceType == 'E') {
          legacy11 = 0xF8;
        }
        else {
          legacy11 = 0xF9;
        }
        legacy12 = (actionElement.deviceNum * 2) + 1;  // Shift left one bit, fill with '1'
        if (actionElement.cmdType == 'D') {
          legacy13 = 0x72;   // Railsounds Dialog Trigger
        }
        else if (actionElement.cmdType == 'F') {
          legacy13 = 0x74;   // Railsounds Effecs Triggers
        }
        else if (actionElement.cmdType == 'C') {
          legacy13 = 0x7C;   // Effects Controls
        }
        // Note: legacy21 and legacy31 are pre-defined; always "FB"
        legacy22 = (actionElement.deviceNum * 2);   // If it's an Engine, okay as-is
        if (actionElement.deviceType == 'T') {     // If it's a Train, set bit to "1"
          legacy22 = legacy22 + 1;
        }
        legacy23 = actionElement.parm1;
        legacy32 = legacy22;
        // Lionel Legacy command checksum.
        legacy33 = legacyChecksum(legacy12, legacy13, legacy22, legacy23, legacy32);
        sprintf(lcdString, "%c %i EXTENDED %i", actionElement.deviceType, actionElement.deviceNum, actionElement.parm1);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
          legacyCmdBufEnqueue(legacy21);
          legacyCmdBufEnqueue(legacy22);
          legacyCmdBufEnqueue(legacy23);
          legacyCmdBufEnqueue(legacy31);
          legacyCmdBufEnqueue(legacy32);
          legacyCmdBufEnqueue(legacy33);
        }
        // Don't bother with legacyCmdBufTransmit() / immediate execution since we have 3 sets and not urgent anyway
        break;
      case 'T':  // TMCC command (only Action types supported here, and only Engines not Trains)
        // Support is here for TMCC StationSounds Diner cars.  Would be very easy to add further TMCC support,
        // for Trains, and for the Extended and Speed commands if ever needed.
        sprintf(lcdString, "%.20s", "TMCC (Action)");
        sendToLCD(lcdString);
        Serial.println(lcdString);
        legacy11 = 0xFE;
        legacy12 = (actionElement.deviceNum >> 1);  // Could just do logical && with 11001000 to make it Train not Engine
        legacy13 = (actionElement.deviceNum << 7) + actionElement.parm1;
        sprintf(lcdString, "TMCC %X %X %X", legacy11, legacy12, legacy13);
        sendToLCD(lcdString);
        Serial.println(lcdString);
        for (byte i = 1; i <= LEGACY_REPEATS; i++) {
          legacyCmdBufEnqueue(legacy11);
          legacyCmdBufEnqueue(legacy12);
          legacyCmdBufEnqueue(legacy13);
        }
        legacyCmdBufTransmit();  // attempt immediate execution
        break;
      case 'Y':  // Accessory on or off, needs special handling -- open or close a relay, not a Legacy/TMCC command.


        // Code here to turn an accessory relay on or off



        break;
    }    // end of switch block

  }   // end of "if we have a Delayed Action table record to process block

  // VERY IMPORTANT to put a call to legacyCmdBufTransmit() right after this loop, and repeat every time following above
  // code to populate legacy buffer, because some commands might populate the buffer at almost the same time, and not
  // be able to be executed until LEGACY_MIN_INTERVAL has passed.  Also the 3-byte extended commands require basically
  // three times through this loop() in order for all three sets of 3 bytes to be sent to Legacy.
  // Not to mention LEGACY_REPEATS will double or triple the number of commands in the buffer, and we can only do 3 at a time.
  legacyCmdBufTransmit();  // Send 3 bytes to Legacy, if available in buffer.

}   // end of loop()

// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

void initializePinIO() {
  // Rev 1/10/17: Initialize all of the input and output pins for A-LEG
  digitalWrite(PIN_SPEAKER, HIGH);  // Piezo
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_LED, HIGH);      // Built-in LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_FRAM1, HIGH);    // Chip Select (CS): pull low to enable
  pinMode(PIN_FRAM1, OUTPUT);
  digitalWrite(PIN_FRAM2, HIGH);    // Chip Select (CS): pull low to enable
  pinMode(PIN_FRAM2, OUTPUT);
  digitalWrite(PIN_FRAM3, HIGH);    // Chip Select (CS): pull low to enable
  pinMode(PIN_FRAM3, OUTPUT);
  pinMode(PIN_REQ_TX_A_LEG, INPUT_PULLUP);   // A-LEG will pull this pin LOW when it wants to send A-MAS a message
  digitalWrite(PIN_HALT, HIGH);
  pinMode(PIN_HALT, INPUT);                  // HALT pin: monitor if gets pulled LOW it means someone tripped HALT.  Or change to output mode and pull LOW if we want to trip HALT.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
  pinMode(PIN_PANEL_BROWN_ON, INPUT_PULLUP);  // Pulled LOW when selected on control panel.  Same with following...
  pinMode(PIN_PANEL_BROWN_OFF, INPUT_PULLUP);
  pinMode(PIN_PANEL_BLUE_ON, INPUT_PULLUP);
  pinMode(PIN_PANEL_BLUE_OFF, INPUT_PULLUP);
  pinMode(PIN_PANEL_RED_ON, INPUT_PULLUP);
  pinMode(PIN_PANEL_RED_OFF, INPUT_PULLUP);
  pinMode(PIN_PANEL_4_ON, INPUT_PULLUP);
  pinMode(PIN_PANEL_4_OFF, INPUT_PULLUP);
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

void populateDelayedActionTable() {
  // Rev 01-16-17.  Populate Delayed Action table with some test data.  This is ONLY CALLED when running in test mode, can delete any time.
  const unsigned int delayedActionTestRecs = 50;   // Just a guess at how many records we might need in all, probably will need 400 or so eventually
  // Create an array of Delayed Action records for test data...
  delayedAction actionArray[delayedActionTestRecs] = {
    // Sensor/Timer/Expired, Sensor_Num, 0=Clear/1=Trip, Time_Ripe, Eng/Train/Accy/Switch/Route, Device_Num, Cmd_Type, Parm_1, Parm_2
    // Cmd_Type: E, A, M, S, B, D, F, C, T, or Y
    // Whistle/Horn signals are as follows.  "o" for short sound, "=" for long sound:
    //   o        When stopped, air brakes applied, pressure equalized.  Use to signal that train has stopped in station.  Couple seconds after tripping dest exit sensor.
    //   =        Also used when approaching station.  Use when trip destination block entry sensor.
    //   o o      Ack. of any communications i.e. an order to start the train forward.  Use when get okay to proceed.
    //   = =      Release brakes and proceed.  Use just after getting okay to proceed, just before moving.
    //   = = o =  Approaching grade crossing.  Use when approaching grade crossing from either direction, when trip block entry sensor.
    //   o o o    When stopped, going to back up.  Only used in Park mode.
    { 'T',0,0,5000,'E',14,'B',251,0 },   // Long start-up sequence
    { 'T',0,0,20000,'E',14,'D',8,0 },    // Engineer request permission to depart
    { 'T',0,0,33000,'E',14,'B',245,0 },  // Turn on bell
    { 'T',0,0,36000,'E',14,'B',165,0 },  // RPMs 5
    { 'T',0,0,37000,'E',14,'M',4,0 },    // Momentum 4
    { 'T',0,0,37000,'E',14,'A',120,0 },  // Abs speed 120
    { 'T',0,0,39000,'E',14,'B',244,0 },  // Bell off
    // 1/14/17: Horn is basic command 28.  Let's try basic command 224+[0..15] for quilling horn intensity.  225 sounds great, per my notes.
    // Based on tests on small steam, Big Boy, and F-unit horn blasts, short and long, low and loud, it seems like I need to store
    // some data unique to each loco as to how to time various horn blasts.  I.e. quilling intensity, duration for continuous blow,
    // how quickly I can do short blows, etc.
    // IMPORTANT: Rather than using the generic "Blow Horn 1" basic command 28, we will be using the "Quilling Horn Intensity" basic command
    // 224 + [0..25].  224 is "horn/whistle off" and 225 thru 239 is maximum intensity.  Can ramp up in intensity on long blows.
    { 'T',0,0,41000,'E',14,'B',225,0 },   // Blow horn
    { 'T',0,0,41250,'E',14,'B',228,0 },   // Continuous blows must be not more than 300ms apart, at least on F-unit
    { 'T',0,0,41500,'E',14,'B',231,0 },   // If they are 350ms apart, they become separate short toots on F-unit
    { 'T',0,0,41750,'E',14,'B',234,0 },   // 250ms is still continuous with the 1484 steam loco
    { 'T',0,0,42000,'E',14,'B',237,0 },
    { 'T',0,0,42250,'E',14,'B',239,0 },
    { 'T',0,0,42500,'E',14,'B',239,0 },
    { 'T',0,0,42750,'E',14,'B',239,0 },
    { 'T',0,0,44100,'E',14,'B',225,0 },   // Short horn bursts must be 350ms apart to guarantee non continuous (at least on F-unit)
    { 'T',0,0,44200,'E',14,'B',224,0 },   // Putting a 224 (Quilling Horn 0) seems to stop the horn.  250ms toot plus 200ms pause works great on steam 1484.
                                            // 250ms plus 200ms pause works on Big Boy, but is kind of thumpy, not enough delay.
    { 'T',0,0,44550,'E',14,'B',225,0 },   // If they are 300ms apart, they become continuous on F-unit
    { 'T',0,0,44650,'E',14,'B',224,0 },   // 225 is kind of a quiet horn, 239 is crazy loud, 230 is good on both steam and diesel
    { 'T',0,0,45000,'E',14,'B',225,0 },   // 350 on steam is still continuous -- and louder than the above continuous for some reason
    { 'T',0,0,45100,'E',14,'B',224,0 },
    { 'T',0,0,45450,'E',14,'B',225,0 },   // 400ms on steam is partly separate, partly continuous
    { 'T',0,0,44550,'E',14,'B',224,0 },
    { 'T',0,0,45900,'E',14,'B',239,0 },   // 450ms on steam is goofy -- alternating from low and high volume whistle
    { 'T',0,0,43000,'E',14,'B',160,0 },  // RPMs all the way down
    { 'T',0,0,43500,'E',14,'A',0,0 },    // Abs speed zero
    { 'T',0,0,51000,'E',14,'B',253,0 },
    { 'T',0,0,55000,'E',14,'B',254,0 },
    // These are TMCC commands, used only for StationSounds Diner dialogue at this point.
    { 'T',0,0, 1500,'E',1,'T',23,0 },     // 23 = numberic 7 = departure announcement (w/out Aux 1)
    { 'T',0,0,10000,'E',1,'T', 9,0 },     // 9 = Aux 1
    { 'T',0,0,10100,'E',1,'T',23,0 },    // 23 = numeric 7 = arrival announcement (w/ Aux 1)
    { 'T',0,0,1000,'E',91,'A',1,0 },    // Eng 91 (Powermaster) Abs speed 1 = ON
    { 'T',0,0,60000,'E',91,'A',0,0 },   // Eng 91 (Powermaster) Abs speed 0 = OFF
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 },
    { 'E',0,0,0,'E',0,' ',0,0 }
  };
  // Transfer the contents of our test-data array into the FRAM2 Delayed Action table.
  for (unsigned int delayedActionRec = 0; delayedActionRec < delayedActionTestRecs; delayedActionRec++) {  // Using offset 0 for ease of calcs
    actionElement = actionArray[delayedActionRec];
    writeActionElement();
  }
  return;
}

void writeActionElement() {             // Add a new record to the Delayed Action table.
  // Insert the record in the first Expired (available) element in Delayed Action table, or add a new record at the end.
  // totalDelayedActionRecs tracks the number of records that have been written at some point, even if expired.
  // First find an empty slot, either an expired record or a new record at the end...
  unsigned int availableDelayedActionRec = 0;
  while (availableDelayedActionRec < totalDelayedActionRecs) {  // See if we have an expired record we can write to
    delayedAction tempElement;   // To read status of existing records, so we don't step on record we are supposed to write
    unsigned long FRAM2Address = FRAM2_ACTION_START + (availableDelayedActionRec * FRAM2_ACTION_LEN);  // Address of record number to check
    byte b[FRAM2_ACTION_LEN];  // create a byte array to hold one Delayed Action record
    FRAM2.read(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_read, data
    memcpy(&tempElement, b, FRAM2_ACTION_LEN);
    // Now tempElement has been populated with the record just read.
    if (tempElement.status == 'E') {            // Expired record -- great, we will write here!
      break;
    }
    availableDelayedActionRec++;
  }
  // availableDelayedActionRec will now hold the record number we need to write to.
  // If all previous records are unavailable, be sure to increment totalDelayedActionRecs
  if (availableDelayedActionRec == totalDelayedActionRecs) {     // Account for zero-offset record numbers vs. actual number of records occupied
    totalDelayedActionRecs++;   // We will expand the size of the table by one element
    Serial.print("Expanded Delayed Action table to "); Serial.print(totalDelayedActionRecs); Serial.println(" records.");
  }
  // Now write the record to FRAM2 at record availableDelayedActionRec...
  // FRAM2.write requires a  buffer of type BYTE, not char or structure, so convert.
  // Addresses must be specified in UNSIGNED LONG (not UNSIGNED INT)
  unsigned long FRAM2Address = FRAM2_ACTION_START + (availableDelayedActionRec * FRAM2_ACTION_LEN);  // Address of record number to check
  if (FRAM2Address > (FRAM2Top - FRAM2_ACTION_LEN)) {    // Overflowed FRAM2!
    sprintf(lcdString, "%.20s", "FRAM 2 overflow!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(6);
  }
  byte b[FRAM2_ACTION_LEN];  // create a byte array to hold one Delayed Action record
  memcpy(b, &actionElement, FRAM2_ACTION_LEN);     // Use this in final code to init each action element to Expired status.
  FRAM2.write(FRAM2Address, FRAM2_ACTION_LEN, b);  // (address, number_of_bytes_to_write, data
  return;
}

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

bool legacyCmdBufIsEmpty() {
  // Rev: 09/29/17
  return (legacyCmdBufCount == 0);
}

bool legacyCmdBufIsFull() {
  // Rev: 09/29/17
  return (legacyCmdBufCount == LEGACY_BUF_ELEMENTS);
}

void legacyCmdBufEnqueue(const byte d) {
  // Rev: 09/29/17.  Insert a byte at the head of the Legacy command buffer, for later transmission to Legacy.
  if (!legacyCmdBufIsFull()) {
    legacyCmdBuf[legacyCmdBufHead] = d;
    legacyCmdBufHead = (legacyCmdBufHead + 1) % LEGACY_BUF_ELEMENTS;
    legacyCmdBufCount++;
  } else {       // buffer overflow; LEGACY_BUF_ELEMENTS is too small.
    sprintf(lcdString, "%.20s", "Legacy buf overflow!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(6);
  }
  return;
}

byte legacyCmdBufDequeue() {
  // Rev: 09/29/17.  Retrieve a record, if any, from the Legacy command buffer.
  byte b = 0;
  if (!legacyCmdBufIsEmpty()) {
    b = legacyCmdBuf[legacyCmdBufTail];
    legacyCmdBufTail = (legacyCmdBufTail + 1) % LEGACY_BUF_ELEMENTS;
    legacyCmdBufCount--;
  } else {  // Programming error, because Dequeue() must only be called when buffer is not empty.
    sprintf(lcdString, "%.20s", "legacyCmdBufGet err!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(7);  // Should never hit this!
  }
  return b;
}

void legacyCmdBufTransmit() {
  // Rev: 09/29/17.  If minimum interval has elapsed, transmit one 3-byte TMCC or Legacy command.
  // We don't implement LEGACY_REPEATS here because some commands, such as BLOW HORN, should probably only be transmitted once in order to preserve
  // the timing.  Other commands such as VOL UP might be adjusted more than intended.  We can decide for sure with some live testing... *************************
  // Also, some commands are 9 bytes, including SMOKE OFF, SMOKE HIGH, and many dialogue tracks.
  // No problem calling this function when buffer is empty; just doesn't do anything.
  // Legacy commands comprised of 9 bytes will be handed by three separate calls.
  static unsigned long legacyLastTransmit = millis();
  byte b;
  if (!legacyCmdBufIsEmpty()) {
    if ((millis() - legacyLastTransmit) > LEGACY_MIN_INTERVAL_MS) {    // Enough time since last transmit, so okay to transmit again
      b = legacyCmdBufDequeue();   // Get first byte of the command
      if (b==0xF8 || b==0xF9 || b==0xFB) {    // It's a Legacy command
      } else if (b == 0xFE) {    // It's a TMCC command
      } else {
        Serial.print("FATAL ERROR.  Unexpected data in Legacy buffer: ");
        Serial.println(b, HEX);
        endWithFlashingLED(7);  // Should never hit this!
      }
      Serial3.write(b);      // Write 1st of 3 bytes
      b = legacyCmdBufDequeue();    // Get the 2nd byte of 3
      Serial3.write(b);      // Write 2nd of 3 bytes
      b = legacyCmdBufDequeue();    // Get the 3rd byte of 3
      Serial3.write(b);      // Write 3rd of 3 bytes
      legacyLastTransmit = millis();
      shortChirp();          // Do a little chirp just so I can hear when a command is sent to Legacy, i.e. in relation to when I see a train hit a sensor, to assess latency.
    }
    return;
  }
}

void checkIfPowerMasterOnOffPressed() {            // Check the four control panel "PowerMaster" on/off switches to turn power on or off
  if (digitalRead(PIN_PANEL_BROWN_ON) == LOW)  {   // Is the operator pressing the control panel "Brown track power on" button at this moment?
    // create actionElement to turn engine 91 absolute speed 1
    // then add it as a new Delayed Action table record by calling a function
    actionElement.status = 'T';         // Timer record
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();  // Execute asap
    actionElement.deviceType = 'E';     // Powermasters are treated as Engines by Legacy
    actionElement.deviceNum = POWERMASTER_1_ID;
    actionElement.cmdType = 'A';    // Absolute speed turns on (1) or off (0)
    actionElement.parm1 = 1;            // Abs speed 1 to turn on
    actionElement.parm2 = 0;            // n/a
    writeActionElement();               // Send this action to the Delayed Action table; we will execute as a separate step
    // Block until operator releases switch.  Very bad form because you hold up everything, disaster in Auto/Park mode.
    // Could fix with more programming to avoid blocking code, or ignore when in Auto or Park mode.
    // But no good reason to use these track power switches in Auto/Park mode, so just leave it as-is for now.
    // Also, this command won't be sent to Legacy until operator releases the switch.
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_BROWN_ON) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_BROWN_OFF) == LOW)  {
    // create actionElement to turn engine 91 absolute speed 0
    // then add it as a new Delayed Action table record by calling a function
    actionElement.status = 'T';         // Timer record
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();  // Execute asap
    actionElement.deviceType = 'E';     // Powermasters are treated as Engines by Legacy
    actionElement.deviceNum = POWERMASTER_1_ID;
    actionElement.cmdType = 'A';    // Absolute speed turns on (1) or off (0)
    actionElement.parm1 = 0;            // Abs speed 0 to turn off
    actionElement.parm2 = 0;            // n/a
    writeActionElement();               // Send this action to the Delayed Action table; we will execute as a separate step
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_BROWN_OFF) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_BLUE_ON) == LOW)  {
    actionElement.status = 'T';
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_2_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 1;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_BLUE_ON) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_BLUE_OFF) == LOW)  {
    actionElement.status = 'T';
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_2_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 0;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_BLUE_OFF) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_RED_ON) == LOW)  {
    actionElement.status = 'T';
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_3_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 1;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_RED_ON) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_RED_OFF) == LOW)  {
    actionElement.status = 'T';
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_3_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 0;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_RED_OFF) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_4_ON) == LOW)  {
    actionElement.status = 'T';         // Timer record
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_4_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 1;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_4_ON) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  if (digitalRead(PIN_PANEL_4_OFF) == LOW)  {
    actionElement.status = 'T';         // Timer record
    actionElement.sensorNum = 0;
    actionElement.sensorTripType = 0;
    actionElement.timeRipe = millis();
    actionElement.deviceType = 'E';
    actionElement.deviceNum = POWERMASTER_4_ID;
    actionElement.cmdType = 'A';
    actionElement.parm1 = 0;
    actionElement.parm2 = 0;
    writeActionElement();
    delay(20);  // wait for press debounce
    while (digitalRead(PIN_PANEL_4_OFF) == LOW) { }   // Pause everything while held
    delay(20);  // wait for release debounce
  }
  return;
}

void requestLegacyHalt() {
  // Rev 08/21/17: Pulled code out of requestEmergencyStop and checkIfHaltPinPulledLow
  // We are going to force this rather than using the Legacy command buffer, because this is an emergency stop.
  for (int i = 0; i < 3; i++) {   // Send Halt to Legacy (3 times for good measure)
    legacy11 = 0xFE;
    legacy12 = 0xFF;
    legacy13 = 0xFF;
    Serial3.write(legacy11);
    Serial3.write(legacy12);
    Serial3.write(legacy13);
    delay(LEGACY_MIN_INTERVAL_MS);  // Brief (25ms) pause between bursts of 3-byte commands
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

bool RS485fromMAStoLEG_SmokeMessage() {
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'S') {  // It's a Smoke on/off command
        return true;
      }
    }
  }
  return false;
}

bool RS485GetSmokeOn() {
  bool s;  // Smoke on?
  if (RS485MsgIncoming[4] == 'Y') {  // Smoke ON
     s = true;
     sprintf(lcdString, "Smoke ON");
  } else {
    s = false;
    sprintf(lcdString, "Smoke OFF");
  }
  sendToLCD(lcdString);
  return s;
}

bool RS485fromMAStoLEG_FastOrSlowStartupMessage() {
  // Returns true if this is a message telling us to use fast versus slow startup
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'F') {  // It's a startup fast/slow command
        return true;
      }
    }
  }
  return false;
}

bool RS485GetSlowStartup() {
  // Returns true if operator wants slow startup, false if they want fast startup
  bool s;   // Slow startup?
  if (RS485MsgIncoming[4] == 'S') {  // Slow startup
    s = true;
    sprintf(lcdString, "Slow startup");
  } else {
    s = false;
    sprintf(lcdString, "Fast startup");
  }
  sendToLCD(lcdString);
  return s;  // slowStartup gets set true or false
}

bool RS485fromMAStoLEG_RouteMessage() {
  // Returns true if this is a command from A-MAS assigning a new route for a given train
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_LEG) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_MAS) {
      if (RS485MsgIncoming[3] == 'R') {  // It's a "new route" command
        return true;
      }
    }
  }
  return false;
}

bool RS485fromOCCtoMAS_RegistrationMessage() {
  // Returns true if this message is an incoming "new train registered" message from A-OCC (to A-MAS)
  if (RS485MsgIncoming[RS485_TO_OFFSET] == ARDUINO_MAS) {
    if (RS485MsgIncoming[RS485_FROM_OFFSET] == ARDUINO_OCC) {
      if (RS485MsgIncoming[3] == 'R') {  // A new train has been identified by operator and needs to be registered
        return true;
      }
    }
  }
  return false;
}

bool RS485fromOCCtoMAS_TrainRecord() {    // N means that this is a train data record; otherwise it's an "all done" record with no train data.
  return (RS485MsgIncoming[7] == 'N');
}

// 9/30/17: THIS FUNCTION SHOULD PROBABLY PASS NOTHING AND RETURN A 5-BYTE STRUCT (Train, Block, Dir, Entry, Exit)
void RS485fromOCCtoMAS_ExtractData(byte * t, byte * b, char * d, byte * n, byte * x) {
  // Returns train number, block number, direction, entry sensor, and exit sensor, using data in RS485MsgIncoming.
  // Could be re-written to pass nothing and return a 5-byte struct...???
  * t = RS485MsgIncoming[4];  // Train number: 1..8 -- we must assume that any other block is static (train 99)
  * b = RS485MsgIncoming[5];  // Block number 1..32 that the train is in
  * d = RS485MsgIncoming[6];  // Direction E or W that the train is facing
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
    sendToLCD(lcdString);
    Serial.print(lcdString);
    endWithFlashingLED(6);
  }
  return;
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

void initializeFRAM2() {
  // Rev 01/13/17.  Don't even bother with version; just make sure it's a working FRAM.

  if (FRAM2.begin()) {       // Any non-zero result is an error
    sprintf(lcdString, "%.20s", "FRAM2 bad response.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(1);
  }

  if (FRAM2.getControlBlockSize() != 128) {
    sprintf(lcdString, "%.20s", "FRAM2 bad blk size.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    endWithFlashingLED(1);
  }

  FRAM2BufSize = FRAM2.getMaxBufferSize();  // Should return 128 w/ modified library value
  FRAM2Bottom = FRAM2.getBottomAddress();      // Should return 128 also
  FRAM2Top = FRAM2.getTopAddress();            // Should return 8191
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
  // Rev 10/05/16: Version for A-LEG only: calls requestLegacyHalt() *and* releases accessory relays.
  requestLegacyHalt();
  initializeShiftRegisterPins();  // Release all relay coils that might be holding accessories on
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

void shortChirp() {   // Shorter for use when signalling data sent to Legacy
  // Rev 10/05/16
  digitalWrite(PIN_SPEAKER, LOW);  // turn on piezo
  delay(2);
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
  // Rev 01/20/17: Special version for A-LEG calls requestLegacyHalt()
  // Rev 10/29/16
  // Check to see if another Arduino, or Emergency Stop button, pulled the HALT pin low.
  // If we get a false HALT due to turnout solenoid EMF, it only lasts for about 8 microseconds.
  // So we'll sit in a loop and check twice to confirm if it's a real halt or not.
  // Rev 10/19/16 extend delay from 50 microseconds to 1 millisecond to try to eliminate false trips.
  // No big deal because this only happens very rarely.
  if (digitalRead(PIN_HALT) == LOW) {   // See if it lasts a while
    delay(1);  // Pause to let a spike resolve
    if (digitalRead(PIN_HALT) == LOW) {  // If still low, we have a legit Halt, not a neg voltage spike
      requestLegacyHalt();
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

