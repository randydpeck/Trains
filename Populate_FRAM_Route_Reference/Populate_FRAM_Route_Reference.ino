// POPULATE FRAM 1 Rev: 12/03/17.

// Include the following #define if we want to run the system with just the lower-level track.  Comment out to create records for both levels of track.
//#define SINGLE_LEVEL     // Comment this out for full double-level routes.  Use it for single-level route testing.

byte FRAM1Version[3] = {12, 03, 17 };  // Date will be placed in FRAM1 control block as the first three bytes
byte FRAM2Version[3] = { 9, 16, 17 };  // Date will be placed in FRAM2 control block as the first three bytes

// Arduino memory overflow if we try to populate all three tables at once, so split the code into two parts.
// ONE of the following TWO #define statements must be commented out, and the other must be included:
#define COMPILE_ROUTE      // To compile and populate FRAM1 with the Route Reference table
//#define COMPILE_PARK       // To compile and populate FRAM1 with the two route "Park" tables

// Populate FRAM 1 Route Reference, Park 1, and Park 2 tables.
// Can be compiled for Single Level or Double Level.
// Can be compiled to populate Route Reference, or Park 1 and Park 2 Reference.
// Be sure to comment in or out, the COMPILE_ROUTE, COMPILE_PARK, and/or SINGLE_LEVEL define statements as appropriate.

// 12/03/17: Added Orig Town, Dest Town, Max Train Len, and Route Restrictions to Park 1 and Park 2 tables.  
//           8K FRAM: Total used 8139 bytes of 8192 capacity.
//           If we run out of FRAM in the future, due to a larger layout or even one more route, we can simply store the two
//           park routes on a separate FRAM.  No big deal since they stack; just need one more wire from Arduino for chip select.
// 09/17/17: Moved 1st-level reversing routes to end of routes from respective origin sidings, so reversing from CCW to CW will be minimized.
//           When operating in single-level mode, once you start traveling CW, you CANNOT get back to moving CCW on any route.
//           There also seemed to be some missing route options, added several to single-level route table.
// 10/17/16: Various tweaks by Randy including new FRAM library identify chip as MB85RS64 (not MB85RS64V.)
// 09/12/16: Inserted "last known turnout position" block of 4 bytes into FRAM1 control block.
//           Added Max Train Length as parameter to Route Reference table.
// 09/09/16: Minor cleanup, ready for single-level route reference population (but no dest siding length at this point)
// To populate FRAM1, first run it with COMPILE_ROUTE uncommented and COMPILE_PARK commented, then run it vice-versa.  Now the whole thing is populated.
// 08/15/16: Write entire Route Reference table to FRAM1
// 08/15/16: Added SINGLE_LEVEL #define blocks, so that we can create a Route Reference table with ONLY
//           bottom-level routes, versus routes that use the entire layout (including upper level.)
// 07/13/16: SPEED: Without Serial.print statements, the code reads the entire Route Reference table
//           (70 records) in 25 milliseconds = .0025 seconds.  Wow!  Virtually instantaneous.
// 07/13/16: Added IFDEF statements to conditionally compile into two parts due to memory overflow
// 07/12/16: Updated Hackscribble FRAM library to 128-byte max buffer size
// 07/12/16: This version updated with the latest PRIMARY Route Reference table.
// Due to Arudino 8K internal RAM, we may have to split the code that populates FRAM1 with the other
// two tables (both used in Park mode) to another piece of code.  Not sure yet.

// Note that I modified the Hackscribble_ferro (FRAM) library (.h only) to change buffer size from 64 to 128 bytes.
// This was necessary since the Route Reference "Park 1" records are 118 bytes long (more than 64 bytes.)
// I can save some memory if I go with the standard 64 byte buffer and read records a byte at a time, though a bit slower.

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Prerequisites for Hackscribble_Ferro
//////////////////////////////////////////////////////////////////////////////////////////////////////

// FRAM1 control block contents as follows:
// Bytes 0..2 are the version (date) MM, DD, YY
// Bytes 3..6 are 32 bits of last-known-turnout positions.  0 = Normal, 1 = Reverse.  Init to all Normal.
// Bytes 7+ are 10 records for last-known-train-location table.
// Beginning with the first byte of regular FRAM (byte 128), we will store three tables:
// 1. The Route Reference table
// 2. The Park 1 Reference table
// 3. The Park 2 Reference table

const byte PIN_FRAM1           = 11;  // Digital pin 11 is CS for FRAM1, for Route Reference table used by many, and last-known-state of all trains
const byte PIN_FRAM2           = 12;  // FRAM2 used by A-LEG for Event Reference and Delayed Action tables

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
#include "Hackscribble_Ferro.h"
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
byte               FRAM1GotVersion[3]       = {  0,  0,  0 };  // This will hold the version retrieved from FRAM1, to confirm matches version above.
const unsigned int FRAM1_ROUTE_START        = 128;  // Start writing FRAM1 Route Reference table data at this memory address, the lowest available address
const byte         FRAM1_ROUTE_REC_LEN      =  77;  // Each element of the Route Reference table is 77 bytes long
#ifdef SINGLE_LEVEL
  const byte       FRAM1_ROUTE_RECS         =  32;  // Number of records in the basic Route Reference table, not including "reverse" routes
#else
  const byte       FRAM1_ROUTE_RECS         =  70;  // Number of records in the basic Route Reference table, not including "reverse" routes
#endif
const unsigned int FRAM1_PARK1_START        = FRAM1_ROUTE_START + (FRAM1_ROUTE_REC_LEN * FRAM1_ROUTE_RECS);
const byte         FRAM1_PARK1_REC_LEN      = 127;  // Was 118 until 12/3/17
const byte         FRAM1_PARK1_RECS         =  19;
const unsigned int FRAM1_PARK2_START        = FRAM1_PARK1_START + (FRAM1_PARK1_REC_LEN * FRAM1_PARK1_RECS);
const byte         FRAM1_PARK2_REC_LEN      =  52;  // Was 43 until 12/3/17
const byte         FRAM1_PARK2_RECS         =   4;
Hackscribble_Ferro FRAM1(MB85RS64, PIN_FRAM1);   // Create the FRAM1 object!
// FRAM2 control block (first 128 bytes) contains no data - we don't need a version number because we don't have any initial data to read.
// FRAM2 stores the Delayed Action table.  Table is 12 bytes per record, perhaps 400 records => approx. 5K bytes.
byte               FRAM2ControlBuf[FRAM_CONTROL_BUF_SIZE];
unsigned int       FRAM2BufSize             =   0;  // We will retrieve this via a function call, but it better be 128!
unsigned long      FRAM2Bottom              =   0;  // Should be 128 (address 0..127)
unsigned long      FRAM2Top                 =   0;  // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)
byte               FRAM2GotVersion[3]       = {  0,  0,  0 };  // This will hold the version retrieved from FRAM2, to confirm matches version above.
Hackscribble_Ferro FRAM2(MB85RS64, PIN_FRAM2);  // Create the FRAM2 object!
// Repeat above for FRAM3 if we need a third FRAM

const unsigned int FRAM1_TOP_ADDRESS = FRAM1_PARK2_START + (FRAM1_PARK2_REC_LEN * FRAM1_PARK2_RECS) - 1;

const byte FRAM2_ACTION_START = 128;      // Address in FRAM2 memory to start writing our Delayed Action table
const byte FRAM2_ACTION_LEN = 12;         // Each record in the Delayed Action table is 12 bytes long
const unsigned int FRAM2_ACTION_RECS = 50;   // Just a guess at how many records we might need in all, probably will need 400 or so eventually

unsigned int FRAM2TopRec = 0;          // This will keep track of the highest record number required by the Delayed Action table.

// *** DEFINE STRUCTURES FOR ROUTE REFERENCE TABLES...
// Note 9/12/16 added maxTrainLen which is typically the destination siding length.  But we may prefer to look this up in the
// Block Reference table rather than duplicating the number here???  If duplicated here, both tables must have the same value.
// Destination siding length is used for two things:
// 1. By A-MAS when searching for a route, to be test if the current train will fit in the destination siding;
// 2. By A-LEG when calculating how long to continue into a destination siding before beginning to slow down.
// A-LED needs route information for when it sees commands to set all turnouts for a given route (regular, park1, or park2.)
// Other times, A-LED will simply see a command to set an individual turnout to either Normal or Reverse, or a bit string.

struct routeReference {  // Each element is 77 bytes.  70 routes = 5390 bytes.  32 routes = 2464 bytes.
  byte routeNum;         // Redundant but what the heck.  1 byte
  char originSiding[5];  // 4 chars + null terminator = 5 bytes
  byte originTown;       // 1 byte
  char destSiding[5];    // 4 chars + null terminator = 5 bytes
  byte destTown;         // 1 byte
  byte maxTrainLen;      // In inches.  1 byte
  char restrictions[6];  // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;      // Entry sensor number of the dest siding - where we start slowing down.  1 byte
  byte exitSensor;       // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
  char route[11][5];     // Blocks and Turnouts, up to 11 per route. 0 thru 10, each with 4 chars (plus \n).  55 bytes
};
routeReference routeElement;  // Use this to hold individual elements when retrieved

struct park1Reference {  // Each element is 127 bytes, total of 19 records = 2413 bytes.
  byte routeNum;         // Redundant but what the heck.  1 byte
  char originSiding[5];  // 4 chars + null terminator = 5 bytes
  byte originTown;       // 1 byte
  char destSiding[5];    // 4 chars + null terminator = 5 bytes
  byte destTown;         // 1 byte
  byte maxTrainLen;      // In inches.  1 byte
  char restrictions[6];  // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;      // Entry sensor number of the dest siding - where we start slowing down.  1 byte
  byte exitSensor;       // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
  char route[21][5];     // Blocks and Turnouts, up to 21 per route. 0 thru 20, each with 4 chars (plus \n).  105 bytes
};

// Old version updated 12/2/17 to add originTown, destTown, maxTrainLen, and restrictions
// struct park1Reference {  // Each element is 118 bytes, total of 19 records = 2242 bytes.
//   byte routeNum;         // Redundant but what the heck.  1 byte
//   char originSiding[5];  // 4 chars + null terminator = 5 bytes
//   char destSiding[5];    // 4 chars + null terminator = 5 bytes
//   byte entrySensor;      // Entry sensor number of the dest siding - where we start slowing down.  1 byte
//   byte exitSensor;       // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
//   char route[21][5];     // Blocks and Turnouts, up to 21 per route. 0 thru 20, each with 4 chars (plus \n).  105 bytes
// };

park1Reference park1Element;  // Use this to hold individual elements when retrieved

struct park2Reference {  // Each record 52 bytes long, total of just 4 records = 208 bytes
  byte routeNum;         // Redundant but what the heck.  1 byte
  char originSiding[5];  // 4 chars + null terminator = 5 bytes
  byte originTown;       // 1 byte
  char destSiding[5];    // 4 chars + null terminator = 5 bytes
  byte destTown;         // 1 byte
  byte maxTrainLen;      // In inches.  1 byte
  char restrictions[6];  // Possible future use.  5 char + null terminator = 6 bytes
  byte entrySensor;      // Entry sensor number of the dest siding - where we start slowing down.  1 byte
  byte exitSensor;       // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
  char route[6][5];      // Blocks and Turnouts, up to 6 per route.  0 thru 5, with 4 chars (plus \n).  30 bytes.
};

// Old version updated 12/2/17 to add originTown, destTown, maxTrainLen, and restrictions
// struct park2Reference {  // Each record 43 bytes long, total of just 4 records = 172 bytes
//   byte routeNum;         // Redundant but what the heck.  1 byte
//   char originSiding[5];  // 4 chars + null terminator = 5 bytes
//   char destSiding[5];    // 4 chars + null terminator = 5 bytes
//   byte entrySensor;      // Entry sensor number of the dest siding - where we start slowing down.  1 byte
//   byte exitSensor;       // Exit sensor number of the dest siding - where we do a Stop Immediate.  1 byte
//   char route[6][5];      // Blocks and Turnouts, up to 6 per route.  0 thru 5, with 4 chars (plus \n).  30 bytes.
// };

park2Reference park2Element;  // Use this to hold individual elements when retrieved

#ifdef COMPILE_ROUTE

  #ifdef SINGLE_LEVEL  

    // There are 32 possible routes that involve LEVEL 1 ONLY.
    // Want to keep things moving CCW as much as possible, because once we start moving CW there's no way to get back to CCW.
    routeReference routeArray[FRAM1_ROUTE_RECS] = {
      { 1,"B01E",1,"B13W",3,0,"     ",18,17,"T06N","B21E","T21N","B13W","    ","    ","    ","    ","    ","    ","    "},
      { 2,"B01E",1,"B14W",4,0,"     ",14,13,"T06N","B21E","T21R","B16W","T20R","T19N","B14W","    ","    ","    ","    "},
      { 3,"B01E",1,"B15W",4,0,"     ",16,15,"T06N","B21E","T21R","B16W","T20N","B15W","    ","    ","    ","    ","    "},
      { 4,"B01W",1,"B13E",3,0,"     ",17,18,"T01N","B08W","T16N","B13E","    ","    ","    ","    ","    ","    ","    "},
      { 5,"B01W",1,"B14E",4,0,"     ",13,14,"T01N","B08W","T16R","B12E","T17N","T18R","T19N","B14E","    ","    ","    "},
      { 6,"B01W",1,"B15E",4,0,"     ",15,16,"T01N","B08W","T16R","B12E","T17R","B15E","    ","    ","    ","    ","    "},
      { 7,"B02E",1,"B05W",2,0,"     ",10, 9,"T08N","T07N","B07W","T15R","T14N","B05W","    ","    ","    ","    ","    "},  // Reverses CCW to CW
      { 8,"B02E",1,"B06W",2,0,"     ",12,11,"T08N","T07N","B07W","T15R","T14R","B06W","    ","    ","    ","    ","    "},  // Reverses CCW to CW
      { 9,"B02E",1,"B13W",3,0,"     ",18,17,"T08N","T07R","T06R","B21E","T21N","B13W","    ","    ","    ","    ","    "},
      {10,"B02E",1,"B14W",4,0,"     ",14,13,"T08N","T07R","T06R","B21E","T21R","B16W","T20R","T19N","B14W","    ","    "},
      {11,"B02E",1,"B15W",4,0,"     ",16,15,"T08N","T07R","T06R","B21E","T21R","B16W","T20N","B15W","    ","    ","    "},
      {12,"B02W",1,"B13E",3,0,"     ",17,18,"T03R","T01R","B08W","T16N","B13E","    ","    ","    ","    ","    ","    "},
      {13,"B02W",1,"B14E",4,0,"     ",13,14,"T03R","T01R","B08W","T16R","B12E","T17N","T18R","T19N","B14E","    ","    "},
      {14,"B02W",1,"B15E",4,0,"     ",15,16,"T03R","T01R","B08W","T16R","B12E","T17R","B15E","    ","    ","    ","    "},
      {15,"B05E",2,"B02W",1,0,"     ", 4, 3,"T14N","T15R","B07E","T07N","T08N","B02W","    ","    ","    ","    ","    "},
      {16,"B05W",2,"B14E",4,0,"     ",13,14,"T12R","T11R","B11W","T18N","T19N","B14E","    ","    ","    ","    ","    "},
      {17,"B06E",2,"B02W",1,0,"     ", 4, 3,"T14R","T15R","B07E","T07N","T08N","B02W","    ","    ","    ","    ","    "},
      {18,"B06W",2,"B14E",4,0,"     ",13,14,"T13R","T12N","T11R","B11W","T18N","T19N","B14E","    ","    ","    ","    "},
      {19,"B13E",3,"B01W",1,0,"     ", 2, 1,"T21N","B21W","T06N","B01W","    ","    ","    ","    ","    ","    ","    "},
      {20,"B13E",3,"B02W",1,0,"     ", 4, 3,"T21N","B21W","T06R","T07R","T08N","B02W","    ","    ","    ","    ","    "},
      {21,"B13W",3,"B01E",1,0,"     ", 1, 2,"T16N","B08E","T01N","B01E","    ","    ","    ","    ","    ","    ","    "},
      {22,"B13W",3,"B02E",1,0,"     ", 3, 4,"T16N","B08E","T01R","T03R","B02E","    ","    ","    ","    ","    ","    "},
      {23,"B14E",4,"B01W",1,0,"     ", 2, 1,"T20R","B16E","T21R","B21W","T06N","B01W","    ","    ","    ","    ","    "},
      {24,"B14E",4,"B02W",1,0,"     ", 4, 3,"T20R","B16E","T21R","B21W","T06R","T07R","T08N","B02W","    ","    ","    "},
      {25,"B14W",4,"B01E",1,0,"     ", 1, 2,"T18R","T17N","B12W","T16R","B08E","T01N","B01E","    ","    ","    ","    "},
      {26,"B14W",4,"B02E",1,0,"     ", 3, 4,"T18R","T17N","B12W","T16R","B08E","T01R","T03R","B02E","    ","    ","    "},
      {27,"B14W",4,"B05E",2,0,"     ", 9,10,"T18N","B11E","T11R","T12R","B05E","    ","    ","    ","    ","    ","    "},  // Reverses CCW to CW
      {28,"B14W",4,"B06E",2,0,"     ",11,12,"T18N","B11E","T11R","T12N","T13R","B06E","    ","    ","    ","    ","    "},  // Reverses CCW to CW
      {29,"B15E",4,"B01W",1,0,"     ", 2, 1,"T20N","B16E","T21R","B21W","T06N","B01W","    ","    ","    ","    ","    "},
      {30,"B15E",4,"B02W",1,0,"     ", 4, 3,"T20N","B16E","T21R","B21W","T06R","T07R","T08N","B02W","    ","    ","    "},
      {31,"B15W",4,"B01E",1,0,"     ", 1, 2,"T17R","B12W","T16R","B08E","T01N","B01E","    ","    ","    ","    ","    "},
      {32,"B15W",4,"B02E",1,0,"     ", 3, 4,"T17R","B12W","T16R","B08E","T01R","T03R","B02E","    ","    ","    ","    "}
    };

  #else   // SINGLE_LEVEL is *NOT* defined

    //  The "full" (dual-level) Route Reference table mirrors the one in the Excel spreadsheet, because we want to
    //  encourage trains reversing from CW to CCW, and vice versa.
    //  9/16/17: Rearranged B02E-B04W, B03E-B04W, B04E-B02W, and B04E-B03W to be last choice of their origins, since they lead
    //           to the same town that they started at (they all start and end at town 1.)  Now matches Excel spreadsheet also.
    routeReference routeArray[FRAM1_ROUTE_RECS] = {    // There are currently 70 records in the Route Reference table
      { 1,"B01E",1,"B13W",3,0,"     ",18,17,"T06N","B21E","T21N","B13W","    ","    ","    ","    ","    ","    ","    "},
      { 2,"B01E",1,"B14W",4,0,"     ",14,13,"T06N","B21E","T21R","B16W","T20R","T19N","B14W","    ","    ","    ","    "},
      { 3,"B01E",1,"B15W",4,0,"     ",16,15,"T06N","B21E","T21R","B16W","T20N","B15W","    ","    ","    ","    ","    "},
      { 4,"B01W",1,"B13E",3,0,"     ",17,18,"T01N","B08W","T16N","B13E","    ","    ","    ","    ","    ","    ","    "},
      { 5,"B01W",1,"B14E",4,0,"     ",13,14,"T01N","B08W","T16R","B12E","T17N","T18R","T19N","B14E","    ","    ","    "},
      { 6,"B01W",1,"B15E",4,0,"     ",15,16,"T01N","B08W","T16R","B12E","T17R","B15E","    ","    ","    ","    ","    "},
      { 7,"B02E",1,"B05W",2,0,"     ",10, 9,"T08N","T07N","B07W","T15R","T14N","B05W","    ","    ","    ","    ","    "},
      { 8,"B02E",1,"B06W",2,0,"     ",12,11,"T08N","T07N","B07W","T15R","T14R","B06W","    ","    ","    ","    ","    "},
      { 9,"B02E",1,"B13W",3,0,"     ",18,17,"T08N","T07R","T06R","B21E","T21N","B13W","    ","    ","    ","    ","    "},
      {10,"B02E",1,"B14W",4,0,"     ",14,13,"T08N","T07R","T06R","B21E","T21R","B16W","T20R","T19N","B14W","    ","    "},
      {11,"B02E",1,"B15W",4,0,"     ",16,15,"T08N","T07R","T06R","B21E","T21R","B16W","T20N","B15W","    ","    ","    "},
      {12,"B02E",1,"B04W",1,0,"     ", 8, 7,"T08N","T07N","B07W","T15N","T09R","B04W","    ","    ","    ","    ","    "},  // Starts and ends at Town 1
      {13,"B02W",1,"B19E",5,0,"     ",19,20,"T03N","T02N","B09W","T23N","T24N","T25N","T26R","T28N","B17E","T29R","B19E"},
      {14,"B02W",1,"B20E",5,0,"     ",21,22,"T03N","T02N","B09W","T23N","T24N","T25N","T26R","T28N","B17E","T29N","B20E"},
      {15,"B02W",1,"B13E",3,0,"     ",17,18,"T03R","T01R","B08W","T16N","B13E","    ","    ","    ","    ","    ","    "},
      {16,"B02W",1,"B14E",4,0,"     ",13,14,"T03R","T01R","B08W","T16R","B12E","T17N","T18R","T19N","B14E","    ","    "},
      {17,"B02W",1,"B15E",4,0,"     ",15,16,"T03R","T01R","B08W","T16R","B12E","T17R","B15E","    ","    ","    ","    "},
      {18,"B03E",1,"B05W",2,0,"     ",10, 9,"T08R","T07N","B07W","T15R","T14N","B05W","    ","    ","    ","    ","    "},
      {19,"B03E",1,"B06W",2,0,"     ",12,11,"T08R","T07N","B07W","T15R","T14R","B06W","    ","    ","    ","    ","    "},
      {20,"B03E",1,"B13W",3,0,"     ",18,17,"T08R","T07R","T06R","B21E","T21N","B13W","    ","    ","    ","    ","    "},
      {21,"B03E",1,"B14W",4,0,"     ",14,13,"T08R","T07R","T06R","B21E","T21R","B16W","T20R","T19N","B14W","    ","    "},
      {22,"B03E",1,"B15W",4,0,"     ",16,15,"T08R","T07R","T06R","B21E","T21R","B16W","T20N","B15W","    ","    ","    "},
      {23,"B03E",1,"B04W",1,0,"     ", 8, 7,"T08R","T07N","B07W","T15N","T09R","B04W","    ","    ","    ","    ","    "},  // Starts and ends at Town 1
      {24,"B03W",1,"B19E",5,0,"     ",19,20,"T05N","T04N","B10W","T27N","T28R","B17E","T29R","B19E","    ","    ","    "},
      {25,"B03W",1,"B19E",5,0,"     ",19,20,"T05R","T02R","B09W","T23N","T24N","T25N","T26R","T28N","B17E","T29R","B19E"},
      {26,"B03W",1,"B19W",5,0,"     ",20,19,"T05N","T04N","B10W","T27R","B18E","T22R","B22W","T30R","B19W","    ","    "},
      {27,"B03W",1,"B20E",5,0,"     ",21,22,"T05N","T04N","B10W","T27N","T28R","B17E","T29N","B20E","    ","    ","    "},
      {28,"B03W",1,"B20E",5,0,"     ",21,22,"T05R","T02R","B09W","T23N","T24N","T25N","T26R","T28N","B17E","T29N","B20E"},
      {29,"B03W",1,"B20W",5,0,"     ",22,21,"T05N","T04N","B10W","T27R","B18E","T22R","B22W","T30N","B20W","    ","    "},
      {30,"B04E",1,"B02W",1,0,"     ", 4, 3,"T15N","B07E","T07N","T08N","B02W","    ","    ","    ","    ","    ","    "},  // Starts and ends at Town 1
      {31,"B04E",1,"B03W",1,0,"     ", 6, 5,"T15N","B07E","T07N","T08R","B03W","    ","    ","    ","    ","    ","    "},  // Starts and ends at Town 1
      {32,"B04W",1,"B19E",5,0,"     ",19,20,"T04R","B10W","T27N","T28R","B17E","T29R","B19E","    ","    ","    ","    "},
      {33,"B04W",1,"B19W",5,0,"     ",20,19,"T04R","B10W","T27R","B18E","T22R","B22W","T30R","B19W","    ","    ","    "},
      {34,"B04W",1,"B20E",5,0,"     ",21,22,"T04R","B10W","T27N","T28R","B17E","T29N","B20E","    ","    ","    ","    "},
      {35,"B04W",1,"B20W",5,0,"     ",22,21,"T04R","B10W","T27R","B18E","T22R","B22W","T30N","B20W","    ","    ","    "},
      {36,"B05E",2,"B02W",1,0,"     ", 4, 3,"T14N","T15R","B07E","T07N","T08N","B02W","    ","    ","    ","    ","    "},
      {37,"B05E",2,"B03W",1,0,"     ", 6, 5,"T14N","T15R","B07E","T07N","T08R","B03W","    ","    ","    ","    ","    "},
      {38,"B05W",2,"B14E",4,0,"     ",13,14,"T12R","T11R","B11W","T18N","T19N","B14E","    ","    ","    ","    ","    "},
      {39,"B06E",2,"B02W",1,0,"     ", 4, 3,"T14R","T15R","B07E","T07N","T08N","B02W","    ","    ","    ","    ","    "},
      {40,"B06E",2,"B03W",1,0,"     ", 6, 5,"T14R","T15R","B07E","T07N","T08R","B03W","    ","    ","    ","    ","    "},
      {41,"B06W",2,"B14E",4,0,"     ",13,14,"T13R","T12N","T11R","B11W","T18N","T19N","B14E","    ","    ","    ","    "},
      {42,"B13E",3,"B01W",1,0,"     ", 2, 1,"T21N","B21W","T06N","B01W","    ","    ","    ","    ","    ","    ","    "},
      {43,"B13E",3,"B02W",1,0,"     ", 4, 3,"T21N","B21W","T06R","T07R","T08N","B02W","    ","    ","    ","    ","    "},
      {44,"B13E",3,"B03W",1,0,"     ", 6, 5,"T21N","B21W","T06R","T07R","T08R","B03W","    ","    ","    ","    ","    "},
      {45,"B13W",3,"B01E",1,0,"     ", 1, 2,"T16N","B08E","T01N","B01E","    ","    ","    ","    ","    ","    ","    "},
      {46,"B13W",3,"B02E",1,0,"     ", 3, 4,"T16N","B08E","T01R","T03R","B02E","    ","    ","    ","    ","    ","    "},
      {47,"B14E",4,"B01W",1,0,"     ", 2, 1,"T20R","B16E","T21R","B21W","T06N","B01W","    ","    ","    ","    ","    "},
      {48,"B14E",4,"B02W",1,0,"     ", 4, 3,"T20R","B16E","T21R","B21W","T06R","T07R","T08N","B02W","    ","    ","    "},
      {49,"B14E",4,"B03W",1,0,"     ", 6, 5,"T20R","B16E","T21R","B21W","T06R","T07R","T08R","B03W","    ","    ","    "},
      {50,"B14W",4,"B01E",1,0,"     ", 1, 2,"T18R","T17N","B12W","T16R","B08E","T01N","B01E","    ","    ","    ","    "},
      {51,"B14W",4,"B02E",1,0,"     ", 3, 4,"T18R","T17N","B12W","T16R","B08E","T01R","T03R","B02E","    ","    ","    "},
      {52,"B14W",4,"B05E",2,0,"     ", 9,10,"T18N","B11E","T11R","T12R","B05E","    ","    ","    ","    ","    ","    "},
      {53,"B14W",4,"B06E",2,0,"     ",11,12,"T18N","B11E","T11R","T12N","T13R","B06E","    ","    ","    ","    ","    "},
      {54,"B15E",4,"B01W",1,0,"     ", 2, 1,"T20N","B16E","T21R","B21W","T06N","B01W","    ","    ","    ","    ","    "},
      {55,"B15E",4,"B02W",1,0,"     ", 4, 3,"T20N","B16E","T21R","B21W","T06R","T07R","T08N","B02W","    ","    ","    "},
      {56,"B15E",4,"B03W",1,0,"     ", 6, 5,"T20N","B16E","T21R","B21W","T06R","T07R","T08R","B03W","    ","    ","    "},
      {57,"B15W",4,"B01E",1,0,"     ", 1, 2,"T17R","B12W","T16R","B08E","T01N","B01E","    ","    ","    ","    ","    "},
      {58,"B15W",4,"B02E",1,0,"     ", 3, 4,"T17R","B12W","T16R","B08E","T01R","T03R","B02E","    ","    ","    ","    "},
      {59,"B19E",5,"B03E",1,0,"     ", 5, 6,"T30R","B22E","T22R","B18W","T27R","B10E","T04N","T05N","B03E","    ","    "},
      {60,"B19E",5,"B04E",1,0,"     ", 7, 8,"T30R","B22E","T22R","B18W","T27R","B10E","T04R","T09R","B04E","    ","    "},
      {61,"B19W",5,"B02E",1,0,"     ", 3, 4,"T29R","B17W","T28N","T26R","T25N","T24N","T23N","B09E","T02N","T03N","B02E"},
      {62,"B19W",5,"B03E",1,0,"     ", 5, 6,"T29R","B17W","T28N","T26R","T25N","T24N","T23N","B09E","T02R","T05R","B03E"},
      {63,"B19W",5,"B03E",1,0,"     ", 5, 6,"T29R","B17W","T28R","T27N","B10E","T04N","T05N","B03E","    ","    ","    "},
      {64,"B19W",5,"B04E",1,0,"     ", 7, 8,"T29R","B17W","T28R","T27N","B10E","T04R","T09R","B04E","    ","    ","    "},
      {65,"B20E",5,"B03E",1,0,"     ", 5, 6,"T30N","B22E","T22R","B18W","T27R","B10E","T04N","T05N","B03E","    ","    "},
      {66,"B20E",5,"B04E",1,0,"     ", 7, 8,"T30N","B22E","T22R","B18W","T27R","B10E","T04R","T09R","B04E","    ","    "},
      {67,"B20W",5,"B02E",1,0,"     ", 3, 4,"T29N","B17W","T28N","T26R","T25N","T24N","T23N","B09E","T02N","T03N","B02E"},
      {68,"B20W",5,"B03E",1,0,"     ", 5, 6,"T29N","B17W","T28N","T26R","T25N","T24N","T23N","B09E","T02R","T05R","B03E"},
      {69,"B20W",5,"B03E",1,0,"     ", 5, 6,"T29N","B17W","T28R","T27N","B10E","T04N","T05N","B03E","    ","    ","    "},
      {70,"B20W",5,"B04E",1,0,"     ", 7, 8,"T29N","B17W","T28R","T27N","B10E","T04R","T09R","B04E","    ","    ","    "}
    };
  #endif   // Single level vs double level

#endif   // COMPILE_ROUTE

#ifdef COMPILE_PARK

  park1Reference park1Array[FRAM1_PARK1_RECS] = {

    { 1,"B02W",1,"B17E",7,0,"     ",45,46,"T03N","T02N","B09W","T23N","T24N","T25N","T26R","T28N","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 2,"B03W",1,"B17E",7,0,"     ",45,46,"T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 3,"B04W",1,"B17E",7,0,"     ",45,46,"T04R","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 4,"B02E",1,"B17E",7,0,"     ",45,46,"T08N","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 5,"B03E",1,"B17E",7,0,"     ",45,46,"T08R","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 6,"B04E",1,"B17E",7,0,"     ",45,46,"T15N","B07E","T07N","T08N","B02W","T03N","T02N","B09W","T23N","T24N","T25N","T26R","T28N","B17E","    ","    ","    ","    ","    ","    ","    "},
    { 7,"B04E",1,"B17E",7,0,"     ",45,46,"T15N","B07E","T07N","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 8,"B13E",3,"B17E",7,0,"     ",45,46,"T21N","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    { 9,"B13W",3,"B17E",7,0,"     ",45,46,"T16N","B08E","T01R","T03R","B02E","T08N","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    "},
    {10,"B01E",1,"B17E",7,0,"     ",45,46,"T06N","B21E","T21N","B13W","T16N","B08E","T01R","T03R","B02E","T08N","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    "},
    {11,"B01W",1,"B17E",7,0,"     ",45,46,"T01N","B08W","T16N","B13E","T21N","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    "},
    {12,"B05E",2,"B17E",7,0,"     ",45,46,"T14N","T15R","B07E","T07N","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    {13,"B06E",2,"B17E",7,0,"     ",45,46,"T14R","T15R","B07E","T07N","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    ","    ","    "},
    {14,"B14E",4,"B17E",7,0,"     ",45,46,"T20R","B16E","T21R","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    "},
    {15,"B15E",4,"B17E",7,0,"     ",45,46,"T20N","B16E","T21R","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    ","    ","    ","    ","    ","    ","    "},
    {16,"B14W",4,"B17E",7,0,"     ",45,46,"T18R","T17N","B12W","T16R","B08E","T01R","T03R","B02E","T08N","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    ","    "},
    {17,"B15W",4,"B17E",7,0,"     ",45,46,"T17R","B12W","T16R","B08E","T01R","T03R","B02E","T08N","T07N","B07W","T15N","T09R","B04W","T04R","B10W","T27N","T28R","B17E","    ","    ","    "},
    {18,"B05W",2,"B17E",7,0,"     ",45,46,"T12R","T11R","B11W","T18N","T19N","B14E","T20R","B16E","T21R","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E","    "},
    {19,"B06W",2,"B17E",7,0,"     ",45,46,"T13R","T12N","T11R","B11W","T18N","T19N","B14E","T20R","B16E","T21R","B21W","T06R","T07R","T08R","B03W","T05N","T04N","B10W","T27N","T28R","B17E"}
  };

  park2Reference park2Array[FRAM1_PARK2_RECS] = {

    { 1,"B17E",7,"B23E",6,0,"     ",24,23,"T28N","T26R","T25N","T24N","T23R","B23E"},
    { 2,"B17E",7,"B24E",6,0,"     ",26,25,"T28N","T26R","T25N","T24R","B24E","    "},
    { 3,"B17E",7,"B25E",6,0,"     ",28,27,"T28N","T26R","T25R","B25E","    ","    "},
    { 4,"B17E",7,"B26E",6,0,"     ",30,29,"T28N","T26N","B26E","    ","    ","    "}
  };

#endif   // COMPILE_PARK

void setup() {
	
  Serial.begin(115200);

  Serial.println("******************");
  Serial.println("******************");
  Serial.println("******************");
  #ifdef COMPILE_ROUTE
      Serial.println("Compiling for REGULAR ROUTES (not Park)...");
  #endif
  #ifdef COMPILE_PARK
    Serial.println("Compiling for PARK ROUTES (not Regular)...");
  #endif
  #ifdef SINGLE_LEVEL
    Serial.println("Compiling SINGLE LEVEL ONLY...");
  #else
    Serial.println("Compiling BOTH LEVELS (not just single)...");
  #endif
  Serial.print("FRAM 1 Version: ");
  for (int i = 0; i < 3; i++) {
    Serial.print(FRAM1Version[i]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("******************");
  Serial.println("******************");
  Serial.println("******************");
  Serial.println();

  Serial.print("Size of route element (should be  77): "); Serial.println(sizeof(routeElement));
  Serial.print("Size of park1 element (should be 127): "); Serial.println(sizeof(park1Element));
  Serial.print("Size of park2 element (should be  52): "); Serial.println(sizeof(park2Element));
  Serial.print("FRAM1 top address written (must not be > 8191): "); Serial.println(FRAM1_TOP_ADDRESS);

  // Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
  // Next statement creates an instance of Ferro using the standard Arduino SS pin and FRAM part number.
  // We use a customized version of the Hackscribble_Ferro library which specifies a control block length of 128 bytes.
  // Other than that, we use the standard library.  Specify the chip as MB85RS64 (*not* MB85RS64V.)
  // You specify a FRAM part number and SS pin number like this ...
  // Could create another instance using a different SS pin number for two FRAM chips
  
  Hackscribble_Ferro FRAM1(MB85RS64, PIN_FRAM1);   // Use digital pin 11 for FRAM #1 chip select
	
  if (FRAM1.begin())  // Any non-zero result is an error
  {
    Serial.println("FRAM response not OK");
    while (1) {};
  }
  Serial.println("FRAM response OK.");

  FRAM1BufSize = FRAM1.getMaxBufferSize();  // Should return 128 w/ modified library value
  FRAM1Bottom = FRAM1.getBottomAddress();     // Should return 128 also
  FRAM1Top = FRAM1.getTopAddress();           // Should return 8191

  Serial.print("Buffer size (should be 128): ");
  Serial.println(FRAM1BufSize);    // result = 128 (with modified library)
  Serial.print("Bottom (should be 128): ");
  Serial.println(FRAM1Bottom);        // result = 128
  Serial.print("Top (should be 8191): ");
  Serial.println(FRAM1Top);           // result = 8191

  // Define and write then read control block (first 128 bytes of FRAM):
  // FRAM1 Control Block:
  // Address 0..2 (3 bytes) = Version number month, date, year i.e. 07, 13, 16
  // Address 3..6 (4 bytes) = Last known position of each turnout.  Bit 0 = Normal, 1 = Reverse.
  //    Update these four bytes EVERY TIME a turnout is thrown, either manually (in Manual or POV mode) or automatically (Auto and Park mode)
  //    Each time A-MAS starts up, read these bytes and send the command to set ALL turnouts as part of setup().  Hopefully, this will
  //       almost always result in turnouts being realigned to their existing state.  But throw them first the opposite way and then back to
  //       the desired alignment, just to be certain they are all correctly aligned and also for cool effect upon startup.
  // Address 7..36 (30 bytes) = Last known train locations for train 1 thru 10: trainNo, blockNo, direction

  if (FRAM1.getControlBlockSize() != 128) {
    Serial.println("FRAM1 bad blk size.");
    while (true) {}
  } else {
    Serial.println("FRAM1 Control block size is 128, as expected.");
  }

  byte FRAM1ControlBuf[128];
  for (byte i = 0; i < 128; i++) {
    FRAM1ControlBuf[i] = 65;                // initialize to junk
  }

  // Defined at top of code: byte FRAM1Version[3] = { 7, 13, 16 };  i.e. July 13, 2016.  Will be placed in FRAM1 control block as the first three bytes (followed by the last-known-train-location table.)
  memcpy(FRAM1ControlBuf, &FRAM1Version, sizeof(FRAM1Version));   // Insert FRAM1 version number (latest date modified) in bytes 0, 1, and 2 of the control block buffer
  // Populate the last-known-turnout-position (4 bytes) with all zeroes...all "Normal"/straight.
  FRAM1ControlBuf[3] = 0;
  FRAM1ControlBuf[4] = 0;
  FRAM1ControlBuf[5] = 0;
  FRAM1ControlBuf[6] = 0;
  // Set up a structure to hold the train number, block number, and direction for the last known position of each train (i.e. after a controlled end of Register, Auto, or Park mode.)
  struct lastLocation {
    byte trainNum;    // 1..10
    byte blockNum;    // 1..26
    char direction;  // E|W
  };
  lastLocation lastLocArray[10] = {    // Defaults for each of the 10 trains
    { 1, 1, 'E' },
    { 2, 1, 'E' },
    { 3, 1, 'E' },
    { 4, 1, 'E' },
    { 5, 1, 'E' },
    { 6, 1, 'E' },
    { 7, 1, 'E' },
    { 8, 1, 'E' },
    { 9, 1, 'E' },
    {10, 1, 'E' }
  };
  memcpy(FRAM1ControlBuf + 7, &lastLocArray, sizeof(lastLocArray));    // Put the last-known-location table following the 3-byte version number, into the control block buffer
  Serial.println("Here are the contents of the control block after initializing with default data:");
  for (byte i = 0; i < 128; i++) {
    Serial.print(FRAM1ControlBuf[i]); Serial.print(" ");
  }
  Serial.println();

  Serial.println("Now writing default data to FRAM1...");
  FRAM1.writeControlBlock(FRAM1ControlBuf);
  Serial.println("Now filling the control block buffer with all 99s...  Here it is:");
  for (byte i = 0; i < 128; i++) {
    FRAM1ControlBuf[i] = 99;
  }
  for (byte i = 0; i < 128; i++) {
    Serial.print(FRAM1ControlBuf[i]); Serial.print(" ");
  }
  Serial.println();

  Serial.println("Now we will read the FRAM1 control block and print it out:");
  FRAM1.readControlBlock(FRAM1ControlBuf);
  for (byte i = 0; i < 128; i++) {
    Serial.print(FRAM1ControlBuf[i]); Serial.print(" ");
  }
  Serial.println();
  Serial.println("End of Control Block code.");
  Serial.println();

#ifdef COMPILE_ROUTE

  Serial.println("Now write the entire Route Reference table to FRAM1.");
  Serial.print("FRAM Route Reference table offset (should be 128): ");
  Serial.println(FRAM1_ROUTE_START);
  Serial.print("FRAM Route Reference table record length (should be 77): ");
  Serial.println(FRAM1_ROUTE_REC_LEN);
  Serial.print("FRAM Route Reference table records (should be 32 or 70): ");
  Serial.println(FRAM1_ROUTE_RECS);
  Serial.print("Size of RouteElement (should be 77): ");
  Serial.println(sizeof(routeElement));
  Serial.print("Size of RouteArray[] (should be 2464 or 5390): ");
  Serial.println(sizeof(routeArray));   // should be 77 bytes * 70 (or 32) records = 5390 (or 2464)

  for (byte recordNo = 0; recordNo < FRAM1_ROUTE_RECS; recordNo++) {  // Using offset 0 for ease of calcs 0..69
    unsigned long FRAMAddress = FRAM1Bottom + (recordNo * FRAM1_ROUTE_REC_LEN);
    Serial.print("Writing to Route address: ");  Serial.print(FRAMAddress); Serial.print(" thru "); Serial.println(FRAMAddress + FRAM1_ROUTE_REC_LEN - 1);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_ROUTE_REC_LEN];  // create a byte array to hold one Route Reference record
    memcpy(b, &routeArray[recordNo], FRAM1_ROUTE_REC_LEN);
    FRAM1.write(FRAMAddress, FRAM1_ROUTE_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
  }

#endif  // COMPILE_ROUTE

#ifdef COMPILE_PARK

  Serial.println("Now write the both Park tables to FRAM1.");
  Serial.print("FRAM Park 1 start address (should be 2592 or 5518): ");
  Serial.println(FRAM1_PARK1_START);
  Serial.print("FRAM Park 1 table record length (should be 127): ");
  Serial.println(FRAM1_PARK1_REC_LEN);
  Serial.print("FRAM Park 1 table records (should be 19): ");
  Serial.println(FRAM1_PARK1_RECS);
  Serial.print("Size of park1Element (should be 127): ");
  Serial.println(sizeof(park1Element));
  Serial.print("Size of park1Array[] (should be 2413): ");
  Serial.println(sizeof(park1Array));

  Serial.print("FRAM Park 2 start address (should be 5005 or 7931): ");
  Serial.println(FRAM1_PARK2_START);
  Serial.print("FRAM Park 2 table record length (should be 52): ");
  Serial.println(FRAM1_PARK2_REC_LEN);
  Serial.print("FRAM Park 2 table records (should be 4): ");
  Serial.println(FRAM1_PARK2_RECS);
  Serial.print("Size of park2Element (should be 52): ");
  Serial.println(sizeof(park2Element));
  Serial.print("Size of park2Array[] (should be 208): ");
  Serial.println(sizeof(park2Array));

  Serial.print("Top address used must be less than 8192! (should be 5212 or 8138): ");
  unsigned int x = FRAM1_PARK2_START + (FRAM1_PARK2_REC_LEN * FRAM1_PARK2_RECS) - 1;
  Serial.println(x);

  for (byte recordNo = 0; recordNo < FRAM1_PARK1_RECS; recordNo++) {
    unsigned long FRAMAddress = FRAM1_PARK1_START + (recordNo * FRAM1_PARK1_REC_LEN);
    Serial.print("Writing to Park 1 address: ");  Serial.print(FRAMAddress); Serial.print(" thru "); Serial.println(FRAMAddress + FRAM1_PARK1_REC_LEN - 1);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_PARK1_REC_LEN];
    memcpy(b, &park1Array[recordNo], FRAM1_PARK1_REC_LEN);
    FRAM1.write(FRAMAddress, FRAM1_PARK1_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
  }

  for (byte recordNo = 0; recordNo < FRAM1_PARK2_RECS; recordNo++) {
    unsigned long FRAMAddress = FRAM1_PARK2_START + (recordNo * FRAM1_PARK2_REC_LEN);
    Serial.print("Writing to Park 2 address: ");  Serial.print(FRAMAddress); Serial.print(" thru "); Serial.println(FRAMAddress + FRAM1_PARK2_REC_LEN - 1);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_PARK2_REC_LEN];
    memcpy(b, &park2Array[recordNo], FRAM1_PARK2_REC_LEN);
    FRAM1.write(FRAMAddress, FRAM1_PARK2_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
  }

#endif   // COMPILE_PARK

  Serial.println("Done with writing the entire FRAM.");

  // Now display the entire table to the serial monitor, just for fun
  // NOTE: 7/13/16: With Serial.prints commented out, simply reading every element of all three tables into memory
  // took a grand total of 33 milliseconds -- .0033 seconds.  Wow!  That's a total of 93 records read from FRAM1.
  
  Serial.println("Here is the Route Reference (primary) table:");

  for (byte recordNo = 0; recordNo < FRAM1_ROUTE_RECS; recordNo++) {  // Using offset 0 for ease of calcs 0..69
    unsigned long FRAMAddress = FRAM1Bottom + (recordNo * FRAM1_ROUTE_REC_LEN);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_ROUTE_REC_LEN];  // create a byte array to hold one Route Reference record
    FRAM1.read(FRAMAddress, FRAM1_ROUTE_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
    memcpy(&routeElement, b, FRAM1_ROUTE_REC_LEN);
    Serial.print(FRAMAddress); Serial.print(": ");
    Serial.print(routeElement.routeNum); Serial.print(", '");
    Serial.print(routeElement.originSiding); Serial.print("', '");
    Serial.print(routeElement.originTown); Serial.print("', '");
    Serial.print(routeElement.destSiding); Serial.print("', '");
    Serial.print(routeElement.destTown); Serial.print("', '");
    Serial.print(routeElement.maxTrainLen); Serial.print("', '");
    Serial.print(routeElement.restrictions); Serial.print("', '");
    Serial.print(routeElement.entrySensor); Serial.print("', '");
    Serial.print(routeElement.exitSensor); Serial.print("'");
    for (byte i = 0; i < 11; i++) {
      Serial.print(", '"); Serial.print(routeElement.route[i]); Serial.print("'");
    }
    Serial.println();
  }

  Serial.println("Here is the Park 1 Reference table:");
  for (byte recordNo = 0; recordNo < FRAM1_PARK1_RECS; recordNo++) {
    unsigned long FRAMAddress = FRAM1_PARK1_START + (recordNo * FRAM1_PARK1_REC_LEN);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_PARK1_REC_LEN];
    FRAM1.read(FRAMAddress, FRAM1_PARK1_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
    memcpy(&park1Element, b, FRAM1_PARK1_REC_LEN);
    Serial.print(FRAMAddress); Serial.print(": ");
    Serial.print(park1Element.routeNum); Serial.print(", '");
    Serial.print(park1Element.originSiding); Serial.print("', '");
    Serial.print(park1Element.originTown); Serial.print("', '");
    Serial.print(park1Element.destSiding); Serial.print("', '");
    Serial.print(park1Element.destTown); Serial.print("', '");
    Serial.print(park1Element.maxTrainLen); Serial.print("', '");
    Serial.print(park1Element.restrictions); Serial.print("', '");
    Serial.print(park1Element.entrySensor); Serial.print("', '");
    Serial.print(park1Element.exitSensor); Serial.print("'");
    for (byte i = 0; i < 21; i++) {
      Serial.print(", '"); Serial.print(park1Element.route[i]); Serial.print("'");
    }
    Serial.println();
  }

  Serial.println("Here is the Park 2 Reference table:");
  for (byte recordNo = 0; recordNo < FRAM1_PARK2_RECS; recordNo++) {
    unsigned long FRAMAddress = FRAM1_PARK2_START + (recordNo * FRAM1_PARK2_REC_LEN);
    // FRAM1.write requires a  buffer of type BYTE, not char or structure, so convert
    byte b[FRAM1_PARK2_REC_LEN];
    FRAM1.read(FRAMAddress, FRAM1_PARK2_REC_LEN, b);  // (address, number_of_bytes_to_write, data    
    memcpy(&park2Element, b, FRAM1_PARK2_REC_LEN);
    Serial.print(FRAMAddress); Serial.print(": ");
    Serial.print(park2Element.routeNum); Serial.print(", '");
    Serial.print(park2Element.originSiding); Serial.print("', '");
    Serial.print(park2Element.originTown); Serial.print("', '");
    Serial.print(park2Element.destSiding); Serial.print("', '");
    Serial.print(park2Element.destTown); Serial.print("', '");
    Serial.print(park2Element.maxTrainLen); Serial.print("', '");
    Serial.print(park2Element.restrictions); Serial.print("', '");
    Serial.print(park2Element.entrySensor); Serial.print("', '");
    Serial.print(park2Element.exitSensor); Serial.print("'");
    for (byte i = 0; i < 6; i++) {
      Serial.print(", '"); Serial.print(park2Element.route[i]); Serial.print("'");
    }
    Serial.println();
  }
}

void loop() 
{

}
