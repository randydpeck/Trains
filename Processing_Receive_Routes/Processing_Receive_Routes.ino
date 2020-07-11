// Rev: 08/31/19.  RECEIVE ROUTES FROM PROCESSING AND POPULATE FRAM vis PC Serial/USB
// DON'T FORGET to write the number of Route Reference records into the control block after they have all been received and written to FRAM! **********************
const byte FRAM1Version[3]    = {  8, 29, 19 };  // Date will be placed in FRAM1 control block as the first three bytes
      byte FRAM1GotVersion[3] = {  0,  0,  0 };  // This will hold the version retrieved from FRAM1, to confirm matches version above.

// not used #define PRINT_ROUTES  // Comment this out to receive routes
//#define RECEIVE_ROUTES  // Comment this out to print routes

//enum tType { LEFT, RIGHT }  // Left-hand or right-hand turnout TYPE (does not change, not related to current orientation Normal|Reverse
//enum sType { OFF, CN, CR, DN, DR }  // Turnout route elements can be Converging|Diverging Normal|Reverse (OFF = unknown or n/a)
//enum bType { OFF, EAST, WEST }      // Route BLOCKS can be either BE = East, or BW = West.
//const byte UNKNOWN = 0;
//const byte EAST    = 1;
//const byte WEST    = 2;

const byte MAX_TRAINS = 10; // This is defined in an include, for regular modules.

const byte COMMA  = 44;  // ASCII field separator in serial transmission
const byte RETURN = 13;  // Carriage Return; ASCII record separator in serial transmission

// Route STEPS can be: CM=Comment (comment lines not actually copied to FRAM from the spreadsheet, but also used to pad unused steps),
//                     Locomotive direction commands: FD=Forward Direction, RD=Reverse Direction,
//                     Block travel direction: BE=Block Eastbound, BW=Block Westbound,
//                     Turnout orientation and travel direction: CN=Converging Normal, CR=Converging Reverse, DN=Diverging Normal, DR=Diverging Reverse.
// I'm going to use type const byte to define these, rather than enum, because 1) enum can be tricky when passed as fn parm, and 2) enums are stored as (2-byte) ints.
// Also note that byte data type is preferred over unsigned char in Arduino, per arduino.cc "for consistency."  char should only be used to store character values.
// bytes are unsigned 0..255, chars are signed -128..127, unsigned chars or unsigned 0..255.
const byte CM = 0;  // CM00.  Comment; any route step that is unused; also to "pad" steps beyond destination of a route record
const byte FD = 1;  // FD00.  Forward Direction (put loco in Forward)
const byte RD = 2;  // RD00.  Reverse Direction (put loco in Reverse)
const byte BE = 3;  // BEnn.  Block #nn traveling East
const byte BW = 4;  // BWnn.  Block #nn traveling West
const byte CN = 5;  // CNnn.  Turnout #nn, Converging from Normal (straight) fork
const byte CR = 6;  // CRnn.  Turnout #nn, Converging from Reverse (curve) fork
const byte DN = 7;  // DNnn.  Turnout #nn, Diverging to Normal (straight) fork
const byte DR = 8;  // DRnn.  Turnout #nn, Diverging to Reverse (curve) fork

// *** SERIAL LCD DISPLAY: The following lines are required by the Digole serial LCD display, connected to serial port 1.
const byte LCD_WIDTH = 20;                     // Number of chars wide on the 20x04 LCD displays on the control panel
#define _Digole_Serial_UART_                   // To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp LCDDisplay(&Serial1, 115200); // UART TX on arduino to RX on module
char lcdString[LCD_WIDTH + 1];                 // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// FRAM1 control block contents as follows:
// Address 0..2 are the version (date) MM, DD, YY
// Address 3..10 are 64 bits of last-known-turnout positions.  0 = Normal, 1 = Reverse.  Init to all Normal.
//   Note: We only need 30 bits for 30 turnouts; this allows up to 64 turnouts for use with a future layout.
// Address 11..40 are 30 bytes containing ten sets of a 3-byte struct (easily expanded for more than 10 trains):
//   byte trainNum i.e. 1..10  Note trainNum assumed to start at #1 (supports up to 256 trains; using 10)
//   byte blockNum i.e. last-known block this train was in i.e. 1..26 (supports up to 256 blocks; using 26)
//   byte whichDirection train is facing i.e. defined const will be EAST (0) or WEST (1)
// Address 41..70 left open for future use, especially if we expand beyond 10 trains above.
// Address 71..72 (2 byte unsigned int) will store the Route Reference table number of records (310 routes as of 8/17/19).
//     Note: This utility will need to write the record length and number of records to the control block *after* storing all applicable Route Reference records in FRAM,
//     because we won't know until all records have been received what total number of records is.  Though the record length *must* be known in advance.

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Prerequisites for Hackscribble_Ferro
//////////////////////////////////////////////////////////////////////////////////////////////////////
// IMPORTANT: Modified FRAM library to change max buffer size from 64 bytes to 128 bytes (0x40 to 0x80.)
// To create an instance of FRAM, we specify a name, the FRAM part number, and our SS pin number, i.e.:
//    Hackscribble_Ferro FRAM1(MB85RS64, FRAM1_PIN); or FRAM1(MB85RS2MT, FRAM1_PIN);
#include <SPI.h>
#include <Hackscribble_Ferro.h>
const    byte FRAM1_PIN = 53;                 // Digital pin 53 is standard CS for FRAM though any pin could be used.
const    int  FRAM1_BLOCK_LEN = 128;          // We'll compare numbers retrieved from FRAM1 to be sure they match this value.
unsigned int  FRAM1BufSize = 0;               // We will retrieve this via a function call, but it better be 128!
unsigned int  FRAM1CtrlBlockSize = 0;         // We will retrieve this via a function call, and better also be 128.
unsigned long FRAM1Bottom = 0;                // First normal writable address, should be 128 (after 0..127 for control buffer)
unsigned long FRAM1Top = 0;                   // Highest address we can write to...should be 262143 for 256KB chip, or 8191 for 8KB chip.
         byte FRAM1CtrlBuf[FRAM1_BLOCK_LEN];  // Array holds contents of FRAM1 control buffer (version num, last-known info, addresses, etc.)

// FRAM1 memory addresses and related:
const unsigned long FRAM1_ADDR_VERSION     =   0;  // FRAM1 *address* where we store the 3-byte version number.
const unsigned long FRAM1_ADDR_TURNOUTS    =   3;  // FRAM1 *address* where we store the last-known turnout configuration bits (using 30 bits as of 8/26/19.)
const unsigned long FRAM1_ADDR_TRAIN_LOCS  =  11;  // FRAM1 *address* where we store the last-known block number and direction of each train (3 bytes/train * 10 trains.)
const unsigned long FRAM1_ADDR_ROUTE_RECS  =  71;  // FRAM1 *address* where we store the number of records in the Route Reference table (2-byte unsigned int.)
const unsigned long FRAM1_ADDR_ROUTE_START = 128;  // FRAM1 *address* where we store the Route Reference table.

      unsigned int  FRAM1_ROUTE_REC_LEN    =   0;  // Each element of the Route Reference table is this many bytes (assigned based on sizeof(struct); 89 as of 8/26/19.)
      unsigned int  FRAM1_ROUTE_NUM_RECS   =   0;  // The total number of route records (we won't know this until after we have finished writing the table.)
const          byte FRAM1_ROUTE_MAX_STEPS  =  40;  // The maximum number of steps in a Route Reference record (using max 37 as of 8/27/19.)
// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
// Next statement creates an instance of Ferro using the standard Arduino SS pin and FRAM part number.
// We use a customized version of the Hackscribble_Ferro library which specifies a control block length of 128 bytes.
// Other than that, we use the standard library.  Specify the chip as MB85RS2MT (previously MB85RS64, and not MB85RS64V.)
Hackscribble_Ferro FRAM1(MB85RS2MT, FRAM1_PIN);


// Create a "union" to swap between two individual bytes, and a 2-byte integer
union {
  byte byteVersion[2];
  unsigned int intVersion;
} bytesAndInt;

// *** DEFINE STRUCTURES FOR ROUTE REFERENCE TABLES...

// The routeStep structure represents any step in the Route Reference table, such as "BE27", "CR06", "RD00", or "CM00" for example.
struct routeStep {
  byte type;             // type is const byte CM, FD, RD, BE, BW, CN, CR, DN, or DR
  byte val;              // val is block number (1..26) or turnout number (1..30) or n/a for CM, FD, and RD
};
routeStep step;

struct routeReference {  // Each element is 83 bytes, 310 routes = 25,730 bytes table size as of 8/26/19.
  unsigned int number;   // Route number ranges from 1 through about 325, NOT IN SORTED ORDER.
  routeStep origin;      // Byte representation of "BE" or "BW" + byte block number.  THIS IS THE PRIMARY SORT KEY.
  routeStep dest;        // Byte representation of "BE" or "BW" + byte block number.  THIS IS THE 4th SORT KEY.
  byte levels;           // 1 or 2, indicating if the route uses the first or both levels.
  char type;             // 'A'uto or 'P'ark type.    THIS IS THE 2nd SORT KEY (Auto routes before Park routes, within each group of origin sidings.)
  byte priority;         // 1 = high priority, through 5 = low priority.  THIS IS THE 3rd SORT KEY (among a given set of Auto or Park records.)
                         // There are a max 40 route steps, which includes (duplicates) origin (.step[0]) and destination (last step, excluding end padding)
                         // We don't need to duplicate origin in the step array, but having origin and dest in the array will simplify a lot of loop code.
  routeStep step[FRAM1_ROUTE_MAX_STEPS];  // .step[n]: block/turnout/direction cmd, block or turnout number.  Using a max of 37 as of 8/27/19, so room for expansion.
};
routeReference route;    // Use this to hold individual routes when retrieved.

// LAST KNOWN TURNOUT POSITION TABLE.  Stored in FRAM1 control buffer at address FRAM1_ADDR_TURNOUTS (currently address 3.)
// Only four bytes = 64 bits.  0 = Normal, 1 = Reverse.  We use only 30 bits (30 turnouts) as of 8/19.
byte lastKnownTurnout[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// TRAIN LAST KNOWN LOCATION TABLE to hold train number, block number, and direction for the last known 
// position of each train (i.e. after a controlled end of Register, Auto, or Park mode.)
// Stored in FRAM1 control buffer at address FRAM1_ADDR_TRAIN_LOCS (currently address 11.)
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
  { 8, 0, ' ' },
  { 9, 0, ' ' },
  {10, 0, ' ' }
};

void setup() 
{
  Serial.begin(115200);
  delay(1000);                          // Give LCD display time to boot
  initializeLCDDisplay();               // Initialize the Digole 20 x 04 LCD display
  sprintf(lcdString, "Prog rev: %02i/%02i/%02i", FRAM1Version[0], FRAM1Version[1], FRAM1Version[2]);
  sendToLCD(lcdString);
  Serial.println(lcdString);
  Serial.println("FRAM Table Content Utility.");
  Serial.println(lcdString);

  initializeFRAM1();

  FRAM1_ROUTE_REC_LEN = sizeof(routeReference);  // Confirmed sizeof(route) == sizeof(routeReference)
  //Serial.print("Buffer size (should be 128): "); Serial.println(FRAM1BufSize);
  //Serial.print("Control block size (should be 128): ");  Serial.println(FRAM1CtrlBlockSize);
  //Serial.print("Memory bottom (should be 128): "); Serial.println(FRAM1Bottom);
  //Serial.print("Memory top (should be 262143 for 256KB, or 8191 for 8KB): "); Serial.println(FRAM1Top);
  //Serial.println("Press Enter to initialize control block with all zeroes...");

#ifdef RECEIVE_ROUTES

  Serial.setTimeout(60000);  // Start this listening program first, then wait up to one minute for Processing program to begin transmitting.
  // First empty any junk in the incoming serial buffer...
  sprintf(lcdString, "Serial receive mode."); sendToLCD(lcdString);
  while (Serial.available()) { Serial.read(); }
  sprintf(lcdString, "In buf clear."); sendToLCD(lcdString);
  unsigned int recordNum = 0;  // This will keep track of Route Reference record number, NOT the same as the route number.
  // Now that we've emptied it, wait until we have a 3-digit ASCII integer followed by a comma...
  while (Serial.available() < 4) { }  // Wait for first three-digit ROUTE NUMBER plus comma delimiter
  unsigned int routeNum = Serial.parseInt();
  sprintf(lcdString, "Got route %03i.", routeNum); sendToLCD(lcdString);
  byte b[sizeof(routeReference)];  // Will hold char array same as route

  while (routeNum > 0) {  // Receive records until Processing sends route number = 0 to signal end of table.
    if (Serial.read() != COMMA) { sprintf(lcdString, "Missing comma 1!"); sendToLCD(lcdString); while (true) { } }
    route.number = routeNum;
    while (Serial.available() < 5) { }  // Wait until a full step ORIGIN sub-record is in the serial input buffer.  I.e. "BE01,BW13"+c/r
    // Convert each set of 4 bytes i.e. "BE01" to a 2-byte "step"...
    char stepType[2];  // Will hold serial-incoming ASCII representation of the step type i.e. "BW"
    char stepVal[2];   // Will hold serial-incoming ASCII representation of the step value i.e. "03"
    Serial.readBytes(stepType, 2);  // i.e. "BW"
    Serial.readBytes(stepVal, 2);   // i.e. "03"
    route.origin.type = convertCharsToType(stepType);  // type will be i.e. 3 if "BE"
    route.origin.val = convertCharsToVal(stepVal);     // val will be a byte value of the number i.e. 1 if "01"
    if (Serial.read() != COMMA) { sprintf(lcdString, "Missing comma 2!"); sendToLCD(lcdString); while (true) { } }
    while (Serial.available() < 5) { }  // Wait until a full step DESTINATION sub-record is in the serial input buffer.  I.e. "BE01,BW13"+c/r
    Serial.readBytes(stepType, 2);  // i.e. "BW"
    Serial.readBytes(stepVal, 2);   // i.e. "03"
    route.dest.type = convertCharsToType(stepType);
    route.dest.val = convertCharsToVal(stepVal);
    if (Serial.read() != COMMA) { sprintf(lcdString, "Missing comma 3!"); sendToLCD(lcdString); while (true) { } }
    while (Serial.available() < 2) { }  // Wait until a one-digit LEVEL (1 or 2), plus comma, is received.
    route.levels = (Serial.read() - '0');  // Will be ASCII '1' or '2'
    if ((route.levels < 1) || (route.levels > 2)) { sprintf(lcdString, "Levels isn't 1 or 2!"); sendToLCD(lcdString); while (true) { } }
    if (Serial.read() != COMMA) { sprintf(lcdString, "Missing comma 4!"); sendToLCD(lcdString); while (true) { } }
    while (Serial.available() < 3) { }  // Wait for 2-char priority i.e. "A1", plus comma.  Stored as char + byte, not char + char
    route.type = Serial.read();  // This will be a char; 'A' or 'P'
    route.priority = (Serial.read() - '0');  // Received as a char; convert to a byte 1..5
    // Now read the rest of the route steps.  We already know the first step, which is the origin
    route.step[0].type = route.origin.type;
    route.step[0].val = route.origin.val;
    for (int i = 1; i < (FRAM1_ROUTE_MAX_STEPS); i++) {
      if (Serial.read() != COMMA) { sprintf(lcdString, "Missing comma 5!"); sendToLCD(lcdString); sprintf(lcdString, "Route %i", routeNum); sendToLCD(lcdString); while (true) { } }
      while (Serial.available() < 5) { }  // Wait until a full step DESTINATION sub-record is in the serial input buffer.  I.e. "BE01,BW13"+c/r
      Serial.readBytes(stepType, 2);  // i.e. "BW"
      Serial.readBytes(stepVal, 2);   // i.e. "03"
      route.step[i].type = convertCharsToType(stepType);  // type will be i.e. 3 if "BE"
      route.step[i].val = convertCharsToVal(stepVal);     // val will be a byte value of the number i.e. 1 if "01"
    }
    // All done receiving a route (or just a route number, if route=0 which is end-of-transmission; the next byte better be a c/r
    if (Serial.read() != RETURN) { sprintf(lcdString, "Missing C/R!"); sendToLCD(lcdString); while (true) { } }
    // All done receiving a single route record ("route"); write it to FRAM.
    unsigned long FRAMAddress = FRAM1_ADDR_ROUTE_START + (recordNum * FRAM1_ROUTE_REC_LEN);
    memcpy(b, &route, FRAM1_ROUTE_REC_LEN);  // Copy all bytes in the route record
    FRAM1.write(FRAMAddress, FRAM1_ROUTE_REC_LEN, b);  // (address, number_of_bytes_to_write, data  **** CAN WE JUST WRITE route INSTEAD OF b??? **********************************************
    // Now get next route number...will be zero if end-of-transmission, else continue ablve
    while (Serial.available() < 4) { }  // Wait for first three-digit ROUTE NUMBER plus comma delimiter
    routeNum = Serial.parseInt();
    sprintf(lcdString, "Got route %03i.", routeNum); sendToLCD(lcdString);
    recordNum++;
  }
  // We just received a routeNum = 0, which is how Processing tells us it's done sending data.
  // We just incremented recordNum, which now would point to the first unused record in FRAM.  But remember, we started with record 0,
  //   so the (just-incremented) value in recordNum does in fact reflect the final number of records inserted.  I.e. 0..324 -> 325 records.
  // Since we are updating FRAM with routes (received from Processing), write fresh control block.
  for (byte i = 0; i < FRAM1CtrlBlockSize; i++) {
    FRAM1CtrlBuf[i] = 0;
  }
  // Insert the version number in the control block.
  for (byte i = 0; i < 3; i++) {
    FRAM1CtrlBuf[FRAM1_ADDR_VERSION + i] = FRAM1Version[i];
  }
  // Write the freshly-reset last-known turnout position of all possible turnouts to the control block.
  for (byte i = 0; i < 8; i++) {
    FRAM1CtrlBuf[FRAM1_ADDR_TURNOUTS + i] = lastKnownTurnout[i];
  }
  // Write the freshly-reset last-known block and direction of each train to the control block.
  for (byte i = 0, j = 0; i < MAX_TRAINS; i++, j=j+3) {
    FRAM1CtrlBuf[FRAM1_ADDR_TRAIN_LOCS + j] = lastTrainLoc[i].trainNum;
    FRAM1CtrlBuf[FRAM1_ADDR_TRAIN_LOCS + j + 1] = lastTrainLoc[i].blockNum;
    FRAM1CtrlBuf[FRAM1_ADDR_TRAIN_LOCS + j + 2] = lastTrainLoc[i].whichDirection;
  }
  // Write the just-determined total number of actual route records to the control block.
  bytesAndInt.intVersion = recordNum;
  FRAM1CtrlBuf[FRAM1_ADDR_ROUTE_RECS] = bytesAndInt.byteVersion[0];
  FRAM1CtrlBuf[FRAM1_ADDR_ROUTE_RECS + 1] = bytesAndInt.byteVersion[1];  
  // Now write the whole control block to FRAM1 (this will be at address zero.)
  FRAM1.writeControlBlock(FRAM1CtrlBuf);

  sprintf(lcdString, "Total recs = %03i", recordNum);
  sendToLCD(lcdString);

  while (true) { }

#else  // Print routes

  // First read control block and display that information...
  // We already have FRAM1CtrlBuf[128]
  for (byte i = 0; i < 3; i++) {
    FRAM1GotVersion[i] = FRAM1CtrlBuf[i];
    if (FRAM1GotVersion[i] != FRAM1Version[i]) {
      sprintf(lcdString, "%.20s", "FRAM1 bad version.");
      sendToLCD(lcdString);
      Serial.println(lcdString);
//      while (true) { }
    }
  }
  sprintf(lcdString, "FRAM1 Rev. %02i/%02i/%02i", FRAM1CtrlBuf[0], FRAM1CtrlBuf[1], FRAM1CtrlBuf[2]);
  sendToLCD(lcdString);
  Serial.println(lcdString);
  bytesAndInt.byteVersion[0] = FRAM1CtrlBuf[FRAM1_ADDR_ROUTE_RECS];
  bytesAndInt.byteVersion[1] = FRAM1CtrlBuf[FRAM1_ADDR_ROUTE_RECS + 1];
  unsigned int  FRAM1_ROUTE_NUM_RECS = bytesAndInt.intVersion;
  sprintf(lcdString, "Route recs = %03i", FRAM1_ROUTE_NUM_RECS);
  sendToLCD(lcdString);
  Serial.println(lcdString);
  Serial.print("FRAM1_ROUTE_REC_LEN = "); Serial.println(FRAM1_ROUTE_REC_LEN);
  Serial.print("sizeof(routeReference) = "); Serial.println(sizeof(routeReference));
  // We also know FRAM1_ROUTE_REC_LEN == sizeof(route) == sizeof(routeReference) = 89 as of 9/1/19
  // We also know FRAM1_ROUTE_MAX_STEPS  =  40
  for (unsigned int recordNum = 0; recordNum < FRAM1_ROUTE_NUM_RECS; recordNum++) {  // For every route in the route table
    unsigned long FRAMAddress = FRAM1_ADDR_ROUTE_START + (recordNum * FRAM1_ROUTE_REC_LEN);
    byte b[FRAM1_ROUTE_REC_LEN];  // Will hold char array same as route - can we just populate route directly??? ****************************************************************
    FRAM1.read(FRAMAddress, FRAM1_ROUTE_REC_LEN, b);
    memcpy(&route, b, FRAM1_ROUTE_REC_LEN);
    sprintf(lcdString, "Rec %03i, ", recordNum); Serial.print(lcdString);
    Serial.print("Route "); sprintf(lcdString, "%03i, Origin ", route.number); Serial.print(lcdString);
    // Serial.write() writes ASCII values of each byte to the serial port, but needs a terminator or a length.
    // Serial.print() prints data in ASCII format but needs a \0 string terminator.  Won't work for dest, origin, etc.
    if (route.origin.type == BE) {
      Serial.print("BE");
    } else if (route.origin.type == BW) {
      Serial.print("BW");
    } else {
      Serial.println("Origin type error."); while (true) { }
    }
    sprintf(lcdString, "%02i, Dest ", route.origin.val); Serial.print(lcdString);
    if (route.dest.type == BE) {
      Serial.print("BE");
    } else if (route.dest.type == BW) {
      Serial.print("BW");
    } else {
      Serial.println("Dest type error."); while (true) { }
    }
    sprintf(lcdString, "%02i, Levels ", route.dest.val); Serial.print(lcdString);
    sprintf(lcdString, "%1i, Priority ", route.levels); Serial.print(lcdString);
    Serial.print(route.type);
    sprintf(lcdString, "%1i.", route.priority); Serial.println(lcdString);
    for (int routeStep = 0; routeStep < FRAM1_ROUTE_MAX_STEPS; routeStep++) {  // For every step of this route
      switch (route.step[routeStep].type) {
        case CM:
          Serial.print(" CM");
          break;
        case FD:
          Serial.print(" FD");
          break;
        case RD:
          Serial.print(" RD");
          break;
        case BE:
          Serial.print(" BE");
          break;
        case BW:
          Serial.print(" BW");
          break;
        case CN:
          Serial.print(" CN");
          break;
        case CR:
          Serial.print(" CR");
          break;
        case DN:
          Serial.print(" DN");
          break;
        case DR:
          Serial.print(" DR");
          break;
        default:
          Serial.print("Error in route.  Step, type: "); Serial.print(routeStep); Serial.print(", "); Serial.println(route.step[routeStep].type); while (true) { }
          break;
      }
      sprintf(lcdString, "%02i ", route.step[routeStep].val); Serial.print(lcdString);
    }
    Serial.println();
  }

#endif
}


void loop() {

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
  if ((nextLine == (char *)NULL) || (strlen(nextLine) > LCD_WIDTH)) while (true) { };
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

void initializeFRAM1() {
  // Rev 09/26/17: Initialize FRAM chip(s), get chip data and control block data including confirm
  // chip rev number matches code rev number, and get last-known-position-and-direction of all trains structure.

  // Here are the possible responses from FRAM1.begin():
  //    ferroOK = 0
  //    ferroBadStartAddress = 1
  //    ferroBadNumberOfBytes = 2
  //    ferroBadFinishAddress = 3
  //    ferroArrayElementTooBig = 4
  //    ferroBadArrayIndex = 5
  //    ferroBadArrayStartAddress = 6
  //    ferroBadResponse = 7
  //    ferroPartNumberMismatch = 8
  //    ferroUnknownError = 99
  int fStat = FRAM1.begin();
  if (fStat != 0)  {  // Any non-zero result is an error.
    Serial.print("FRAM response not OK.  Status = ");
    Serial.println(fStat);
    while (true) { };
  }
  //Serial.println("FRAM response OK.");
  FRAM1BufSize = FRAM1.getMaxBufferSize();           // Should return 128 w/ modified library value
  FRAM1CtrlBlockSize = FRAM1.getControlBlockSize();  // Should also be 128
  FRAM1Bottom = FRAM1.getBottomAddress();            // Should return 128 as first normal memory address (after control block)
  FRAM1Top = FRAM1.getTopAddress();                  // Should return 262143 for 256KB FRAM; 8191 for 8KB FRAM

  if (FRAM1BufSize != FRAM1_BLOCK_LEN) {
    sprintf(lcdString, "%.20s", "FRAM1 bad buf size.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    while (true) { };
  }
  if (FRAM1CtrlBlockSize != FRAM1_BLOCK_LEN) {
    sprintf(lcdString, "%.20s", "FRAM1 bad blk size.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    while (true) { };
  }
  if (FRAM1Bottom != FRAM1_BLOCK_LEN) {
    sprintf(lcdString, "%.20s", "FRAM1 bad bottom.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    while (true) { };
  }
  if (FRAM1Top != 262143) {
    sprintf(lcdString, "%.20s", "FRAM1 bad top.");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    while (true) { };
  }
  FRAM1.readControlBlock(FRAM1CtrlBuf);
  return;
}

byte convertCharsToType(char s[]) {
  // CM, FD, etc. are defined as CONST BYTE at top of code.  See that for explanation/key to meanings.
  if ((s[0] == 'C') && (s[1] == 'M')) {         // Comment
    return CM;  // CM = 0
  } else if ((s[0] == 'F') && (s[1] == 'D')) {  // Forward Direction
    return FD;  // FD = 1
  } else if ((s[0] == 'R') && (s[1] == 'D')) {  // Reverse Direction
    return RD;  // RD = 2
  } else if ((s[0] == 'B') && (s[1] == 'E')) {  // Block Eastbound
    return BE;  // BE = 3
  } else if ((s[0] == 'B') && (s[1] == 'W')) {  // Block Westbound
    return BW;  // BW = 4
  } else if ((s[0] == 'C') && (s[1] == 'N')) {  // Converging Normal turnout
    return CN;  // CN = 5
  } else if ((s[0] == 'C') && (s[1] == 'R')) {  // Converging Reverse turnout
    return CR;  // CR = 6
  } else if ((s[0] == 'D') && (s[1] == 'N')) {  // Diverging Normal turnout
    return DN;  // DN = 7
  } else if ((s[0] == 'D') && (s[1] == 'R')) {  // Diverging Reverse turnout
    return DR;  // DR = 8
  }
  return 0;  // else, error or comment
}

byte convertCharsToVal(char s[]) {
  return (((s[0] - '0') * 10) + (s[1] - '0'));
}
