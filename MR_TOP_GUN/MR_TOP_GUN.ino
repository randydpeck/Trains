// MR_TOP_GUN Rev: 01/18/18.
char APPVERSION[21] = "Mr. Top Gun 01/18/18";

// It will be a good idea to put an inline fuse with the 18VDC supply that gets sent to the manikin's 24VDC relay coil.
// If the software fails somehow, and the 5VDC relay contacts remain closed, then 18VDC will be sent continously to the manikin's relay coil.
// I'm not sure if the relay coil could burn or if it's rated continuous, or even if a fuse would blow if the relay were held on...???
// NEED TO MEASURE CURRENT DRAWN BY THAT RELAY FROM OUR 18VDC WALL WART.

// I'm using an Arduino Mega because I want to have more than one hardware serial port.  This code could
// easily be modified to work with an Arduino Uno or other model using software serial.
// It could also be easily simplified to eliminate use of the 2004 LCD display, mode switch, and other
// features that are not needed if just duplicating original behavior.

// WARNING regarding audio: The original 8-track tape deck used a common negative speaker terminal, so that only three wires were required
// to carry a 2-channel (stereo) audio signal.  The common speaker terminals were wired to one side of the manikin's speaker, and the positive
// audio outputs (which carry the main audio plus "you missed me" on the left channel, and the "you got me" audio on the right channel) are
// sent to the speaker via a SPDT 24VDC relay in the manikin.  So one or the other channel is sent to the speaker positive terminal.
// Unfortunately, most modern amps (which you will require, along with an audio card) don't work this way -- if you connect the two negative
// speaker outputs together, you'll get some of the left channel on the right side, and vice versa.  So you won't get the stereo separation
// which is required for this game to work properly (you don't want to hear "you got me" and "you missed me" at the same time.)
// I modified the circuit in my amp to make it work properly, or you might be able to find a brigeable-output amp that works.

// PIN 0, 1.      SERIAL 0.  PC serial monitor connected to serial port 0 at 115200 baud.
// PIN 18.        SERIAL 1.  2004 LCD display connected at 9600 buad.
// PIN 16, 17.    SERIAL 2.  MP3 Trigger shield connected at its default 38400 baud.
// PIN 14, 15.    SERIAL 3.  NOT USED FOR SERIAL.  Used for other I/O, see below.

// GAME CONTROL PANEL LAYOUT, LEFT TO RIGHT:  (note that all switches up and centered = play using original default game features)
//
// RGB LED for Game Power/Mode.  Red = powered up, in attract mode.  Green = powered up, in game-play mode.
// Pushbutton for Reset.  Grounds Reset pin on Arduino.  Resets Arduino only, not the MP3 Trigger or the manikin.
//   NOTE: The MP3 Trigger does not seem to have a reset pin or a serial command to reset or stop playing.  Since the MP3 Trigger might
//   still be playing something when we press Reset, we should send a "play hello world"  command to the MP3 Trigger upon startup.
// SPST Toggle for Std vs Custom Audio.  Down = open = Standard audio, Up = grounded = Custom audio
// SPST Toggle for Ballyhoo on or off.  Down = open = Ballyhoo off, Up = grounded = Ballyhoo on
// 1P3T Toggle switch for difficulty.  Pos 1 = Easy, Pos 2 = Medium, Pos 3 = Hard.  Pos 1 or 3 gets grounded, or all open = medium difficulty.
// SPST Toggle for Use PIR?  Down = open = Ignore PIR, Up = grounded = Use PIR *if* attract mode/ballyhoo turned on.
// Red LED for PIR.  When using PIR, turns on when motion is sensed.

// SPECIAL CONTINUOUS_PLAY MODE: For testing purposes, if the Ballyhoo toggle is OFF and the PIR toggle is ON (which is a setting that would
// not be valid), the game will start in a mode that starts a game immediately, and continues to play every game sequentially.

// MP3 Trigger audio clip information:
// Clips may have any name, but the first three characters must be digits 000 thru 255.
// The software identifies clips *only* based on the first three characters, which are addressed as a byte value.
// Addition information in the name indicates if it is Standard vs Custom audio, Ballyhoo vs Game audio, and timing data.
// However this embedded-in-the-filename information is for reference only and not used by the software.

// There is one clip with a filename starting with "001" that plays whenever the game is powered up in Custom audio mode.
// It says something like "Hey, who turned on the lights?" just to let you know the game is powered up.

// We will be using the built-in EEPROM to store the next ballyhoo and the next game audio set number.  This is so that we don't always re-play
// the first clips each time the machine is started, but rather do a continuous rotation through every clip even after the machine is turned off.
// For ballyhoo, each time we start a given ballyhoo track, we will write the number of the next-higher
// audio track in memory location 1 (even if we are already at the highest-numbered game.)
// Each time we retrieve the game number to play next, we will check if the number retrieved falls within the expected range. If the number is
// out of range, start with the first track and write a 2.  This essentially acts as a "modulo" rollover on the game number.
// For game audio, we do essentially the same thing, except that we use memory location 2.
// SAMPLE CODE: This writes the number 12 to EEPROM addres 1: EEPROM.write(1, 12);  // EEPROM.write(int, byte);
// Numbers can be in the range of 0..255
// Here’s how you read that data: EEPROM.read(1);  // EEPROM.read(byte);
#include <EEPROM.h>

// The following "seqNum"s refer to the sequential number, starting at 1, not to the number that is embedded in the filename.
// So, for example, there are 14 "tracks" for standard ballyhoo, 1 to 14, with filenames numbered 201..214.
// And for standard game-play, there are just 3 "tracks", 1 to 3, with filenames numbered 010/011/012/013, 020/021/022/023, and 030/031/032/033.
// So for games, by "sequence number" we really mean game number.  I.e. 1, 2, 3, etc.
const byte ADDR_BALLYHOO           =  1;   // The address in EEPROM where we store the next-to-play Ballyhoo audio track number
const byte ADDR_GAME               =  2;   // The address in EEPROM where we store the next-to-play Game audio track number
byte seqNumBallyhoo                =  0;   // Stores the sequence number read from / written to EEPROM during ballyhoo.  0..(numClipsBallyhoo - 1)
byte seqNumGame                    =  0;   // Stores the sequence number read from / written to EEPROM during game play.  0..(numClipsGame - 1)
const byte MAX_RECS_BALLYHOO       = 20;   // The maximum number of ballyhoo audio clips for either standard or custom mode
const byte MAX_RECS_GAME           =  5;   // The maximum number of game audio sequences for either standard or custom mode
byte numClipsBallyhoo              =  0;   // This will be set once we know if we're in standard or custom game mode
byte numClipsGame                  =  0;   // This will also be set once we know if we're in standard or custom game mode.  And by "clips",
                                           // we mean "games."  So for the standard game, numClipsGame = 3 (even though there are 3x4=12 total tracks.)

// *** MISC CONSTANTS AND GLOBALS

const byte PIN_GAME_VS_ATTRACT     =  2;  // INPUT.   Relay contact.  Pulled LOW via N/O relay (controlled by game) to detect game mode else ballyhoo mode.
const byte PIN_LED_ATTRACT_MODE    =  3;  // OUTPUT.  RGB LED 1 game mode RED =   game ON in attract mode.  Pull LOW to turn on.
const byte PIN_LED_GAME_MODE       =  4;  // OUTPUT.  RGB LED 1 game mode GREEN = game ON in game-play mode.  Pull LOW to turn on.
const byte PIN_AUDIO_STD_VS_CUSTOM =  5;  // INPUT.   SPST toggle.  Pulled LOW for CUSTOM AUDIO, default N/C (pullup) for STANDARD AUDIO.  Up = Custom.
const byte PIN_BALLYHOO_ON_VS_OFF  =  6;  // INPUT.   SPST toggle.  Pulled LOW for BALLYHOO ON, default N/C (pullup) for BALLYHOO OFF.  Up = On.
const byte PIN_DIFFICULTY_EASY     =  7;  // INPUT.   1P3T center-off toggle.  Pulled LOW when toggle is LEFT, indicates EASY mode.  Arm raises AFTER manikin says "Fire!"
                                          // NOTE:    Difficulty toggle in CENTER position leaves both EASY and HARD open (floating.)  Infer they want MEDIUM difficulty.
const byte PIN_DIFFICULTY_HARD     =  8;  // INPUT.   1P3T center-off toggle.  Pulled LOW via toggle is RIGHT, indicates HARD mode.  Arm raises BEFORE manikin says "Fire!"
const byte PIN_PIR_USE             =  9;  // INPUT.   SPST toggle.  Pulled LOW if operator wants to use PIR to restrict Ballyhoo to when motion is sensed.  Up = Use PIR.
const byte PIN_PIR_SIGNAL          = 10;  // INPUT.   PIR sensor.  Pulled HIGH by motion.  To detect motion for Ballyhoo on/off.  HC-SR04 range is 1" to 13 feet.
const byte PIN_LED_PIR             = 11;  // OUTPUT.  Red LED 2 turns on when PIR senses motion.  Pull LOW to turn on.
const byte PIN_LED_ON_BOARD        = 12;  // OUPTUT.  Arduino on-board LED.  Pull LOW to turn on.
const byte PIN_ARM_TOGGLE          = 14;  // OUTPUT.  Relay coil.  Set LOW to energize relay coil; contacts send 24vdc pulse to toggle manikin arm.

// ALSO need to wire in a N/O pushbutton to reset the game, necessary to change audio mode maybe

const byte MODE_ATTRACT            =  1;
const byte MODE_GAME               =  2;

const byte AUDIO_CUSTOM            =  1;
const byte AUDIO_STD               =  2;
byte audioMode = AUDIO_STD;               // Default to STANDARD audio track (not my custom audio clips)

const byte BALLYHOO_ON             =  1;
const byte BALLYHOO_OFF            =  2;
byte ballyhooMode = BALLYHOO_ON;           // Default to use ballyhoo (with or without PIR)

const byte DIFFICULTY_EASY         =  1;
const byte DIFFICULTY_MED          =  2;
const byte DIFFICULTY_HARD         =  3;
byte difficultyMode = DIFFICULTY_MED;     // Default to medium difficulty

// gunLiftOffset is the number of ms before (if negative) or after (if positive) the "Fire" command is given, to start raising the gun.
// Easy mode starts *after* the "fire" command; hard mode starts raising the gun *before* he says "fire."
long gunLiftOffset              =     0;  // Will be set below, after we determine difficultyMode.  Can be negative, zero, or positive.
const long OFFSET_MISC          =   300;  // Always add (or subtract) this amount of time for adjustment of audio timing.  Once set, it's done.
const long OFFSET_EASY          =   700;  // In easy mode, pause 1500ms (1.5 sec) *after* he says "fire" before starting to raise gun
const long OFFSET_MED           =     0;  // In normal (medium) mode, start raising the gun the same moment he says "fire."
const long OFFSET_HARD          =  -700;  // In hard mode, start raising the gun 1000ms (1 sec) *before* he actually says "fire."
const long GUN_LIFT_DURATION    =   900;  // Number ms from when gun starts to raise until we should hear the audio gunshot.
                                          // For Medium difficulty, gun starts to raise at the moment we hear "Fire!"

// We keep track of arm position in ballyhoo mode, in case we want to raise/lower it during a ballyhoo.
const byte ARM_DOWN                =  1;
const byte ARM_UP                  =  2;
byte armPosition             = ARM_DOWN;  // Default to assuming we're starting with the arm in the DOWN position
const int DOUBLE_TAP_TIME         = 500;  // Number of ms to wait between taps, for the double-tap that ends game after third round

const byte PIR_ON                  =  1;
const byte PIR_OFF                 =  2;
byte PIRMode = PIR_OFF;                    // Default to not use PIR sensor to enable/disable ballyhoo
bool OkToPlayBallyhoo = false;             // Set to true if all conditions are met to play a ballyhoo track

// PIRMotionExtend is used by PIRMotionUpdate() to help determine if we've seen motion recently enough to allow a ballyhoo
// clip, in cases where we only want to play ballyhoo when motion is detected.
unsigned long PIRMotionExtend  = 180000;   // Number of ms to keep playing ballyhoo after motion is most recently detected

// ballyhooDelay and ballyhooLastPlay help determine if it's TOO SOON to play another ballyhoo clip (even if otherwise allowed)
unsigned long ballyhooDelay    =    5000;  // Number of ms to delay between subsequent ballyhoo audio plays
unsigned long ballyhooLastPlay =       0;  // Time (millis()) that we last played a ballyhoo audio track

bool testMode = false;                     // If set true, starts new game upon startup and runs continuously - for testing purposes only.

// *** SERIAL LCD DISPLAY: The following lines are required by the Digole serial LCD display, connected to serial port 1.
const byte LCD_WIDTH = 20;                 // Number of chars wide on the 20x04 LCD displays on the control panel
#define _Digole_Serial_UART_               // To tell compiler compile the serial communication only
#include <DigoleSerial.h>
DigoleSerialDisp LCDDisplay(&Serial1, 9600); //UART TX on arduino to RX on module
char lcdString[LCD_WIDTH + 1];             // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *****************************************************************************************
// *********************** S T R U C T U R E   D E F I N I T I O N S ***********************
// *****************************************************************************************

// Info on each of the Ballyhoo audio clips.
struct ballyhooAudioStruct {
  byte fileNum;                 // 3-digit file number embedded as first three chars of filename on SD card. i.e. 211 for "211"
  unsigned long liftTime;       // Number of milliseconds into play that he lifts arm for effect; zero to not lift arm (typical)
  unsigned long length;         // Length of this Ballyhoo audio clip in ms
} ballyhooAudio[MAX_RECS_BALLYHOO];

// The following struct ties us into three shootout tries per game, which is fine...
// The time at which the gun is lifted is calculated as an offset (before, at, or after) the "DrawTime", depending on if the game
// is set to easy, medium, or hard play.  Defined above.
struct gameAudioStruct {

  byte introFileNum;             // Game Intro 3-digit file number embedded as first three chars of filename; i.e. 20 for "020"
  unsigned long introLength;     // Game Intro total milliseconds of the shootout audio spiel

  byte game1AFileNum;            // Round 1, Part A ("Ready? Draw!") 3-digit file number; i.e. 21 for "021"
  unsigned long game1ADrawTime;  // Round 1, Part A milliseconds from beginning that clip says "Draw!"
  unsigned long game1ALength;    // Round 1, Part A total milliseconds of audio clip
  byte game1BFileNum;            // Round 1, Part B ("Got me! / Missed me!") 3-digit file number; i.e. 22 for "022"
  unsigned long game1BLength;    // Round 1, Part B total milliseconds of audio clip
  
  byte game2AFileNum;            // Round 2, Part A ("Ready? Draw!") 3-digit file number; i.e. 23 for "023"
  unsigned long game2ADrawTime;  // Round 2, Part A milliseconds from beginning that clip says "Draw!"
  unsigned long game2ALength;    // Round 2, Part A total milliseconds of audio clip
  byte game2BFileNum;            // Round 2, Part B ("Got me! / Missed me!") 3-digit file number; i.e. 24 for "024"
  unsigned long game2BLength;    // Round 2, Part B total milliseconds of audio clip
  
  byte game3AFileNum;            // Round 3, Part A ("Ready? Draw!") 3-digit file number; i.e. 25 for "025"
  unsigned long game3ADrawTime;  // Round 3, Part A milliseconds from beginning that clip says "Draw!"
  unsigned long game3ALength;    // Round 3, Part A total milliseconds of audio clip
  byte game3BFileNum;            // Round 3, Part B ("Got me! / Missed me!") 3-digit file number; i.e. 26 for "026"
  unsigned long game3BLength;    // Round 3, Part B total milliseconds of audio clip
  
} gameAudio[MAX_RECS_GAME];

// *****************************************************************************************
// *****************************************************************************************
// **************************************  S E T U P  **************************************
// *****************************************************************************************
// *****************************************************************************************

void setup() {

  Serial.begin(115200);                 // PC serial monitor window
  // Serial1 is for the Digole 20x4 LCD debug display, already set up
  Serial2.begin(38400);                 // MP3 Trigger card default baud rate is 38400
  initializePinIO();                    // Initialize all of the I/O pins
  delay(1000);                          // Give LCD display time to boot
  initializeLCDDisplay();               // Initialize the Digole 20 x 04 LCD display
  delay(1000);
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  sendToLCD(lcdString);
  Serial.println(lcdString);

  // Set up audio info arrays, depending on if player wants standard audio or custom audio...
  audioMode = getAudioMode();  // Returns AUDIO_STD or AUDIO_CUSTOM

  // FILENAME standards.  Note that the filename ranges are for convenience only, and any file can be assigned any 3-digit number, as long as there
  // is only one file with that number.
  // 001     : Custom game startup phrase "Hey, who turned on the lights?"
  // 010..036: Standard Game clips including intro and various dialogue, 7 separate clips per game.
  // 050..196: Custom Game clips including intro and various dialogue, 7 separate clips per game
  // 201..214: Standard Ballyhoo clips
  // 221..255: Custom Ballyhoo clips

  if (audioMode == AUDIO_STD) {

    numClipsBallyhoo = 14;
    numClipsGame     =  3;

    // There is ONE audio file per ballyhoo (seems obvious, but not the case with game-mode clips, see below.)
    // Ballyhoo audio clips are stored on the MP3 Trigger memory card in the following format:
    // nnn Top Gun [Std|Custom] Ballyhoo [time to draw in .01 sec] [length in .01 sec]
    // "Time to draw" is the time into the audio that the game should raise the gun.  It will be lowered automatically.
    // "Time to draw" does not apply to the standard game; only to the custom game -- and only a few audio clips at that.
    // There are 14 Standard ballyhoo clips, with filename numbers ranging from 201..214
    // i.e. "201 Top Gun Std Ballyhoo 0000 1335.mp3", which is the first standard ballyhoo clip, and is 13.35 seconds long.
    // NOTE: All clip names have times in .01 seconds, whereas the data files have times in .001 seconds (milliseconds.)

    // BALLYHOO: fileNum, liftTime (0 for std audio), length
    ballyhooAudio[ 0] = {201, 0, 13350};
    ballyhooAudio[ 1] = {202, 0, 15600};
    ballyhooAudio[ 2] = {203, 0, 12800};
    ballyhooAudio[ 3] = {204, 0, 13700};
    ballyhooAudio[ 4] = {205, 0, 11350};
    ballyhooAudio[ 5] = {206, 0, 10550};
    ballyhooAudio[ 6] = {207, 0,  9950};
    ballyhooAudio[ 7] = {208, 0,  9400};
    ballyhooAudio[ 8] = {209, 0, 22350};
    ballyhooAudio[ 9] = {210, 0, 14600};
    ballyhooAudio[10] = {211, 0, 10450};
    ballyhooAudio[11] = {212, 0, 15150};
    ballyhooAudio[12] = {213, 0, 13050};
    ballyhooAudio[13] = {214, 0, 15900};

    // There are SEVEN audio files per game: 
    // * An "intro" clip, which says "Hi! Remember to cock the gun."
    // * A pair of clips for each of three rounds per game.
    //   * The first of each pair says "Ready? Draw!" and includes the time at which "Draw!" is spoken, as well as the total clip length.
    //   *  The second clip of the pair says "You got me / You missed me" (on alternate channels) and includes a total length only.
    // There are 3 Standard audio games, of 7 clips each = 21 total clips for standard game-play.
    // Standard game filename numbers range from 010..036.  Each game uses 7 successive numbers i.e. 010,011,012,...,016 for the first game.

    // Game audio GAMEPLAY INTRO clips (1 per game) are stored on the MP3 Trigger memory card in the following format:
    // nnn Top Gun [Std|Custom] Intro [2-digit game no.] [length in .01 sec]
    // i.e. "010 Top Gun Std Intro 01 1060.mp3", which is the intro clip for standard game #1, and is 10.60 seconds long.
    // i.e. "050 Top Gun Custom Intro 01 1270.mp3", which is the intro clip for custom game #1, and is 12.70 seconds long.

    // Game audio GAMEPLAY PART A clips (3 per game) are stored on the MP3 Trigger memory card in the following format:
    // nnn Top Gun [Std|Custom] Game [2-digit game no.] [2-digit sequence 01..03]a ["Fire!" offset in .01 sec.] [length in .01 sec]
    // i.e. "013 Top Gun Std Game 01 02a 0210 0300.mp3", which is the 2nd (of 3) round Part A clip for standard game 1, "Fire!" is said at 2.10sec, total len 3.00 sec.
    // i.e. "055 Top Gun Custom Game 01 03a 0320 0450.mp3", which is the 3rd (of 3) round Part A clip for custom game 1, "Fire!" is said at 3.20 sec, len is 4.50 sec.

    // Game audio GAMEPLAY PART B clips (3 per game) are stored on the MP3 Trigger memory card in the following format:
    // nnn Top Gun [Std|Custom] Game [2-digit game no.]  [2-digit sequence 01..03]b [length in .01 sec]
    // i.e. "014 Top Gun Std Game 01 02b 785.mp3", which is the 2nd (of 3) round Part B clip for standard game 1, total length is 7.85 seconds.
    // i.e. "056 Top Gun Custom Game 01 03b 0870.mp3", which is the 3rd (of 3) round Part B clip for custom game 1, total length is 8.70 seconds.
    
    gameAudio[0] = { 10, 10600, 11, 1250, 1960, 12, 5590, 13, 2100, 3000, 14, 7850, 15, 1650, 2390, 16, 12680};
    gameAudio[1] = { 20, 10250, 21, 1900, 2590, 22, 7630, 23, 1800, 2240, 24, 8130, 25, 1550, 1970, 26, 10040};
    gameAudio[2] = { 30,  9250, 31, 1500, 2080, 32, 9990, 33, 2100, 2750, 34, 8380, 35, 1250, 2010, 36,  9140};

    sprintf(lcdString, "AUDIO MODE: STANDARD");
    sendToLCD(lcdString);
    Serial.println(lcdString);

  } else {  // Assume audioType == AUDIO_CUSTOM

    numClipsBallyhoo = 18;
    numClipsGame     =  4;

    // There are 18 Custom ballhyoo clips, with filename numbers ranging from 221..238
    // i.e. "221 Top Gun Custom Ballyhoo 0550 0940", which is the first custom ballyhoo clip, draw at 5.50 seconds, and is 9.40 seconds long.

    // BALLYHOO: fileNum, liftTime (0 if not arm lift), length
    ballyhooAudio[ 0] = {221,     0, 11600};
    ballyhooAudio[ 1] = {222,     0, 11500};
    ballyhooAudio[ 2] = {223,     0, 11250};
    ballyhooAudio[ 3] = {224, 10000, 12750};
    ballyhooAudio[ 4] = {225,     0,  8500};
    ballyhooAudio[ 5] = {226,     0,  9500};
    ballyhooAudio[ 6] = {227,     0, 13250};
    ballyhooAudio[ 7] = {228,     0, 11500};
    ballyhooAudio[ 8] = {229,     0, 11750};
    ballyhooAudio[ 9] = {230,     0, 13500};
    ballyhooAudio[10] = {231,     0, 14750};
    ballyhooAudio[11] = {232,     0, 15000};
    ballyhooAudio[12] = {233,     0,  7500};
    ballyhooAudio[13] = {234,     0, 14000};
    ballyhooAudio[14] = {235,     0, 14000};
    ballyhooAudio[15] = {236,     0, 15500};
    ballyhooAudio[16] = {237,     0, 14250};
    ballyhooAudio[17] = {238,     0, 17500};

    // There are 4 Custom audio games, of 7 clips each = 28 total clips for custom game-play.
    // Custom game filename numbers range from 050..193.  Each custom game also uses four successive numbers each, i.e. 050, 051, 052, and 053.

    // GAME: introFileNum, introLength, game1AFileNum, game1ADrawTime, game1Length, game1BFileNum, game2ADrawTime, game2Length, game2AFileNum, game3ADrawTime, game3Length
    gameAudio[0] = { 50,  9000, 51, 1500, 2200, 52, 5750, 53,  5000,  5750, 54, 8500, 55,  6400,  7000, 56,  7750};
    gameAudio[1] = { 60, 15000, 61, 3450, 4200, 62, 3750, 63,  6500,  7250, 64, 4800, 65,  4800,  5280, 66, 19250};
    gameAudio[2] = { 70, 11500, 71, 3260, 3940, 72, 6150, 73,  4700,  5340, 74, 4400, 75, 17100, 17650, 76,  8000};
    gameAudio[3] = { 80, 10500, 81, 1600, 2150, 82, 7520, 83, 12320, 12960, 84, 9020, 85,  7980,  8530, 86, 10060};

    // In custom speech mode, we'll play a little "hello world" clip, file number 001...
    // This should stop anything that is playing, in the case where we reset the Arduino (which does not reset the MP3 Trigger.)
    Serial2.write('t');  // 't' tells the MP3 Trigger to play a track, using 3-char number specified by the byte in the next line
    Serial2.write(1);    // Plays 001*.mp3 i.e. "Hey, who turned on the lights?"
    sprintf(lcdString, "AUDIO MODE: CUSTOM");
    sendToLCD(lcdString);
    Serial.println(lcdString);
    delay(3000);         // Give it a few seconds to play

  }

  ballyhooMode = getBallyhooMode();  // Sets to BALLYHOO_ON or BALLYHOO_OFF
  difficultyMode = getDifficultyMode();  // Sets to DIFFICULTY_EASY, DIFFICULTY_MED, or DIFFICULTY_HARD
  if (difficultyMode == DIFFICULTY_EASY) {
    gunLiftOffset = OFFSET_MISC + OFFSET_EASY;   // Will be a positive number
    sprintf(lcdString, "DIFFICULTY: EASY");
  } else if (difficultyMode == DIFFICULTY_MED) {
    gunLiftOffset = OFFSET_MISC + OFFSET_MED;    // Will be zero
    sprintf(lcdString, "DIFFICULTY: MEDIUM");
  } else { // difficultyMode must be DIFFICULTY_HARD
    gunLiftOffset = OFFSET_MISC + OFFSET_HARD;   // Will be a negative number
    sprintf(lcdString, "DIFFICULTY: HARD");
  }
  sendToLCD(lcdString);    // Display difficulty mode on LCD
  Serial.println(lcdString);
  
  PIRMode = getPIRMode();  // Sets to PIR_ON or PIR_OFF
  if (ballyhooMode == BALLYHOO_OFF) {  // We don't even care about PIR mode
    sprintf(lcdString, "BALLYHOO OFF");
  } else if (PIRMode == PIR_ON) {      // Ballyhoo mode is on, and PIR is on
    sprintf(lcdString, "BALLYHOO ON W/ PIR");
  } else {                             // Ballyhoo is on, and PIR is off
    sprintf(lcdString, "BALLYHOO ON W/O PIR");
  }
  sendToLCD(lcdString);    // Display Ballyhoo / PIR mode on LCD
  Serial.println(lcdString);
  
  // See if operator wants special "continuous-play" mode for testing purposes...
  if ((ballyhooMode == BALLYHOO_OFF) && (PIRMode == PIR_ON)) {
    testMode = true;
    sprintf(lcdString, "TEST MODE ON!");
    sendToLCD(lcdString);
    Serial.println(lcdString);
  } else {
    testMode = false;      // Default, set above, but we'll set it again for clarity here
  }

  delay(3000);             // Give operator a few seconds to view LCD display

}

// *****************************************************************************************
// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************
// *****************************************************************************************

void loop() {

  // Look at the relay contacts set by the Manikin to see if we are in attract mode, or game mode
  if (getGameMode() == MODE_ATTRACT) {  // Returns MODE_ATTRACT or MODE_GAME

    // ****************************************************************
    // ****************************************************************
    // ******************* ATTRACT MODE CODE BLOCK ********************
    // ****************************************************************
    // ****************************************************************

    // Illuminate the main game LED (RGB LED 1) in Red to indicate attract mode
    setModeLED(MODE_ATTRACT);
    // Update the "last seen motion" timer, and update the PIR motion LED based on motion and if we are even using the PIR sensor
    PIRMotionUpdate();
    // Assuming we do NOT want to play a ballyhoo, and set true only if we pass necessary criteria.
    OkToPlayBallyhoo = false;

    // If ballyhooMode = BALLYHOO_OFF, then there is nothing to do in ballyhoo mode except sit and wait for the mode to change to game mode.
    if (ballyhooMode == BALLYHOO_ON) {

      // We know we are in a mode that wants ballyhoo to play, but there are several restrictions which we must consider:
      // 1. Is it too soon to play another ballyhoo?
      // 2. Does the operator want to use the PIR sensor to only play ballyhoo when there is motion detected (or for some time after)?
      // 3. If using PIR, has it been too long since we've seen motion?

      if ((ballyhooLastPlay + ballyhooDelay) < millis()) {  // Enough time has passed since last ballyhoo played; so far, so good
        if (PIRMode == PIR_OFF) {  // We want to play regardless of motion detected
          OkToPlayBallyhoo = true;
        } else {  // We need to see if we've had recent motion detected before deciding if okay to play
          if (PIRMotionUpdate() == true) {  // Yay!  We have seen motion recently (even if not at this moment)
            OkToPlayBallyhoo = true;
          }
        }
      }
    }

    if (OkToPlayBallyhoo) {  // Okay, one way or another, we want to play a ballyhoo clip now

      sprintf(lcdString, "BEGIN BALLYHOO MODE");
      sendToLCD(lcdString);
      Serial.println(lcdString);

      // Get the sequence number (0..numClipsBallyhoo - 1) that we want to play.  We will increment this in a moment...
      seqNumBallyhoo = EEPROM.read(ADDR_BALLYHOO);
      // The following takes care of garbage in EEPROM and also modulo roll from highest-numbered-clip back to clip #1:
      if ((seqNumBallyhoo < 0) || (seqNumBallyhoo >= numClipsBallyhoo)) seqNumBallyhoo = 0;
      // Now seqNumBallyhoo gives us the offset into ballyhooAudio[]
      Serial2.write('t');  // 't' tells the MP3 Trigger to play a track, using 3-char number specified by the byte in the next line
      Serial2.write(ballyhooAudio[seqNumBallyhoo].fileNum);  // I.e. 203 means play track named "203" plus anything (.mp3)
      sprintf(lcdString, "Playing Ballyhoo %03i", ballyhooAudio[seqNumBallyhoo].fileNum);
      sendToLCD(lcdString);
      Serial.println(lcdString);

      // Now the audio clip is playing.
      // Define some variables to make our decision blocks easier to understand
      unsigned long startMillis = millis();  // Approx when the clip started playing (now)
      unsigned long liftMillis = startMillis + ballyhooAudio[seqNumBallyhoo].liftTime;  // When the arm should be raised, if at all
      unsigned long completionMillis = startMillis + ballyhooAudio[seqNumBallyhoo].length;  // When clip is done playing
      
      while (millis() < completionMillis) {  // While audio is playing...

        // Keep the motion LED updated, and extend motion expiration time if appropriate...
        PIRMotionUpdate();

        // See if we want to raise the arm at this time...
        if ((armPosition == ARM_DOWN) && (ballyhooAudio[seqNumBallyhoo].liftTime > 0) && (liftMillis < millis())) {
          // We have met all three conditions to indicate that it's time to lift the arm!
          // 1. The arm is in the DOWN position
          // 2. We definitely want to lift the arm at some point (liftTime is non-zero)
          // 3. The time to lift the arm (liftMillis) has passed

          tapArmRelay();  // Raise the arm
          armPosition = ARM_UP;
        }

        // See if player has inserted a coin to break out of ballyhoo mode and start a game...
        if (getGameMode() == MODE_GAME) {  // Player wants to start a game, so break out of this code block
          completionMillis = millis();  // Causes us to break out of our "while" delay loop
        }

      }  // End of "while audio is playing" loop.

      // All done playing this ballyhoo (either because the audio ended, or the player inserted a coin to start a game)
      // If the arm is up, put it down now...
      if (armPosition == ARM_UP) {
        tapArmRelay();  // Lower the arm
        armPosition = ARM_DOWN;
      }

      // Note: Even if we broke out of our loop because user wants to start a real game, ballyhoo will automatically stop playing
      // as soon as the game audio track starts playing (which is done in the MODE_GAME code block, below.)
      // So there is no need to manually stop the ballyhoo from playing, even if it's in the middle of the track.
      // And besides, I can't figure out how to send a serial command to the MP3 Trigger to say "stop playing."

      sprintf(lcdString, "Ballyhoo ended.");
      sendToLCD(lcdString);
      Serial.println(lcdString);

      // Now increment and update the next Ballyhoo to play...
      seqNumBallyhoo++;
      EEPROM.write(ADDR_BALLYHOO, seqNumBallyhoo);
      // Take note of the last time a ballyhoo clip ended, so we'll know how long to delay before playing another...
      ballyhooLastPlay = millis();

      delay(1000);  // Not really needed, just put this here to slow things down

    }  // end of OkToPlayBallyhoo i.e. we want to play a ballyhoo clip

  } else if (getGameMode() == MODE_GAME) {   // We are in game mode, not attract mode

    // **************************************************************
    // **************************************************************
    // ******************** GAME MODE CODE BLOCK ********************
    // **************************************************************
    // **************************************************************

    sprintf(lcdString, "BEGIN GAME MODE");
    sendToLCD(lcdString);
    Serial.println(lcdString);

    // Illuminate the main game LED (RGB LED 1) in Green to indicate game mode
    setModeLED(MODE_GAME);
    // We will ignore motion detection during game play, so turn off the motion LED
    motionLEDOff();

    // Get the sequence number (0..numClipsGame - 1) that we want to play.  We will increment this in a moment...
    seqNumGame = EEPROM.read(ADDR_GAME);
    // The following takes care of garbage in EEPROM and also modulo roll from highest-numbered-clip back to clip #1:
    if ((seqNumGame < 0) || (seqNumGame >= numClipsGame)) seqNumGame = 0;
    // Now seqNumGame gives us the offset into gameAudio[]

    // *********************************************************************************
    // PLAY THE GAME INTRODUCTION...  "Howdy! Remember to cock the gun before you fire."
    // *********************************************************************************

    Serial2.write('t');  // 't' tells the MP3 Trigger to play a track, using 3-char number specified by the byte in the next line
    Serial2.write(gameAudio[seqNumGame].introFileNum);  // This track says "Remember to cock the gun before you draw."
    sprintf(lcdString, "Playing Game %03i", gameAudio[seqNumGame].introFileNum);
    sendToLCD(lcdString);
    Serial.println(lcdString);

    // Define some variables to make our decision blocks easier to understand
    unsigned long startMillis = millis();  // Approx when the clip starts playing (i.e. now)
    unsigned long liftMillis = 0;  // Time at which we should lift the arm, set below (doesn't apply to intro)
    unsigned long completionMillis = startMillis + gameAudio[seqNumGame].introLength;  // When clip will be done playing

    while (millis() < completionMillis) {  // While game intro audio is playing...
      // Just wait; do nothing
    }

    // Game introduction is done!  Wait a moment and then start playing three rounds
    delay(800);
    
    // *********************************************************************************
    // PLAY THE FIRST ROUND...
    // *********************************************************************************

    startMillis = millis();  // Approx when the clip starts playing (i.e. now)
    liftMillis = startMillis + gameAudio[seqNumGame].game1ADrawTime + gunLiftOffset;  // When the arm should be raised
    completionMillis = startMillis + gameAudio[seqNumGame].game1ALength;  // When clip will be done playing

    Serial2.write('t');  // 't' tells the MP3 Trigger to play a track
    Serial2.write(gameAudio[seqNumGame].game1AFileNum);  // This track says "Ready? Draw!"
    sprintf(lcdString, "Round 1 of 3");
    sendToLCD(lcdString);
    Serial.println(lcdString);

    while (millis() < liftMillis) {  // Wait until we need to lift the arm
      // Just wait; do nothing
    }
    tapArmRelay();  // Raise the arm

    // Part A ("Ready? Draw!") is playing (somewhere near the word "Draw!"), and we're now raising the arm...

    // Calculate when we should start playing Part B, which is the gunfire sound + you got me / you missed me...
    // Note: regardless of easy/med/hard mode i.e. raising the arm before, during, or after the word "Draw!", we want the gunshot sound
    // to happen at the moment the gun reaches the raised position.  So this is a fixed time after the gun starts to be raised, and not
    // related to the beginning or ending of the Part A dialogue itself.
    startMillis = liftMillis + GUN_LIFT_DURATION;
    completionMillis = startMillis + gameAudio[seqNumGame].game1BLength;  // When clip will be done playing

    while (millis() < startMillis) {  // While waiting for the gun to get raised to firing position (this works for any mode easy to hard)
      // Just wait; do nothing
    }

    // PLAY THE FIRST ROUND...PART B (blast sound, plus "you hit me" or "you missed")
    Serial2.write('t');
    Serial2.write(gameAudio[seqNumGame].game1BFileNum);  // This track is gun blast, plus "you hit me" or "you missed me"

    while (millis() < completionMillis) {  // Wait until audio is done playing
      // Just wait; do nothing
    }

    // All done playing first round
    tapArmRelay();  // Lower the arm
    delay(800);

    // *********************************************************************************
    // PLAY THE SECOND ROUND...
    // *********************************************************************************

    startMillis = millis();
    liftMillis = startMillis + gameAudio[seqNumGame].game2ADrawTime + gunLiftOffset;
    completionMillis = startMillis + gameAudio[seqNumGame].game2ALength;

    Serial2.write('t');
    Serial2.write(gameAudio[seqNumGame].game2AFileNum);  // This track says "Ready? Draw!"
    sprintf(lcdString, "Round 2 of 3");
    sendToLCD(lcdString);
    Serial.println(lcdString);

    while (millis() < liftMillis) {  // Wait until we need to lift the arm
      // Just wait; do nothing
    }
    tapArmRelay();  // Raise the arm

    // Part A ("Ready? Draw!") is playing (somewhere near the word "Draw!"), and we're now raising the arm...

    // Calculate when we should start playing Part B, which is the gunfire sound + you got me / you missed me...
    startMillis = liftMillis + GUN_LIFT_DURATION;
    completionMillis = startMillis + gameAudio[seqNumGame].game2BLength;  // When clip will be done playing

    while (millis() < startMillis) {  // While waiting for the gun to get raised to firing position (this works for any mode easy to hard)
      // Just wait; do nothing
    }

    // PLAY THE SECOND ROUND...PART B (blast sound, plus "you hit me" or "you missed")
    Serial2.write('t');
    Serial2.write(gameAudio[seqNumGame].game2BFileNum);  // This track is gun blast, plus "you hit me" or "you missed me"

    while (millis() < completionMillis) {  // Wait until audio is done playing
      // Just wait; do nothing
    }

    // All done playing second round
    tapArmRelay();  // Lower the arm
    delay(800);

    // *********************************************************************************
    // PLAY THE THIRD AND FINAL ROUND...INCLUDING DOUBLE TAP RELAY TO END GAME...
    // *********************************************************************************

    startMillis = millis();
    liftMillis = startMillis + gameAudio[seqNumGame].game3ADrawTime + gunLiftOffset;
    completionMillis = startMillis + gameAudio[seqNumGame].game3ALength;

    Serial2.write('t');
    Serial2.write(gameAudio[seqNumGame].game3AFileNum);  // This track says "Ready? Draw!"
    sprintf(lcdString, "Round 3 of 3");
    sendToLCD(lcdString);
    Serial.println(lcdString);

    while (millis() < liftMillis) {  // Wait until we need to lift the arm
      // Just wait; do nothing
    }
    tapArmRelay();  // Raise the arm

    // Part A ("Ready? Draw!") is playing (somewhere near the word "Draw!"), and we're now raising the arm...

    // Calculate when we should start playing Part B, which is the gunfire sound + you got me / you missed me...
    startMillis = liftMillis + GUN_LIFT_DURATION;
    completionMillis = startMillis + gameAudio[seqNumGame].game3BLength;  // When clip will be done playing

    while (millis() < startMillis) {  // While waiting for the gun to get raised to firing position (this works for any mode easy to hard)
      // Just wait; do nothing
    }

    // PLAY THE THIRD ROUND...PART B (blast sound, plus "you hit me" or "you missed")
    Serial2.write('t');
    Serial2.write(gameAudio[seqNumGame].game3BFileNum);  // This track is gun blast, plus "you hit me" or "you missed me"

    while (millis() < completionMillis) {  // Wait until audio is done playing
      // Just wait; do nothing
    }

    // All done playing second round
    tapArmRelay();  // Lower the arm
    if (testMode == false) {   // For normal non-test-mode play, end the game.  Otherwise, play on!
      delay(DOUBLE_TAP_TIME);
      tapArmRelay();  // Ends the game
    }
    delay(800);

    sprintf(lcdString, "GAME OVER.");
    sendToLCD(lcdString);
    Serial.println(lcdString);

    // Now increment and update the next game to play...
    seqNumGame++;
    EEPROM.write(ADDR_GAME, seqNumGame);

    // Prevent a ballyhoo from starting immediately upon game over
    ballyhooLastPlay = millis() + ballyhooDelay;

  }
}  // end of main loop()


// *****************************************************************************************
// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************
// *****************************************************************************************

byte getGameMode() {
  if (digitalRead(PIN_GAME_VS_ATTRACT) == LOW) {  // Relay sending LOW means we must be in game mode
    return MODE_GAME;
  } else {
    return MODE_ATTRACT;
  }
}

byte getAudioMode() {
  if (digitalRead(PIN_AUDIO_STD_VS_CUSTOM) == LOW) {  // Toggle switch set to custom audio
    return AUDIO_CUSTOM;
  } else {
    return AUDIO_STD;
  }
}

byte getBallyhooMode() {
  if (digitalRead(PIN_BALLYHOO_ON_VS_OFF) == LOW) {  // Toggle switch set for Ballyhoo on
    return BALLYHOO_OFF;
  } else {
    return BALLYHOO_ON;
  }
}

byte getDifficultyMode() {
  if (digitalRead(PIN_DIFFICULTY_EASY) == LOW) {
    return DIFFICULTY_EASY;
  } else if (digitalRead(PIN_DIFFICULTY_HARD) == LOW) {
    return DIFFICULTY_HARD;
  } else {  // Assuming that EASY and HARD are both open, defaults to MEDIUM difficulty
    return DIFFICULTY_MED;
  }
}

byte getPIRMode() {
  if (digitalRead(PIN_PIR_USE) == LOW) {  // Toggle switch set to use the PIR feature
    return PIR_ON;
  } else {
    return PIR_OFF;
  }
}

bool PIRMotionUpdate() {
  // Returns true if we care about motion *and* motion has been seen in the last PIRMotionExtend ms.
  // Sets/updates PIRMotionExpires if nothing else.
  static unsigned long PIRMotionExpires = 0;
  // First see if we can update/extend PIRMotionExpires based on *currently* seen motion (if and only if we are even using PIR)
  if ((PIRMode == true) && (digitalRead(PIN_PIR_SIGNAL) == HIGH)) {  // PIR detecting motion right now
    motionLEDOn();
    PIRMotionExpires = millis() + PIRMotionExtend;  // We have a new expiration time
  } else {
    motionLEDOff();
  }
  // We've set the LED to reflect current/recent motion state, and updated motion expiration time if appropriate
  if (PIRMotionExpires > millis()) {  // Have we seen motion recently?
    return true;
  } else {
    return false;
  }
}

void motionLEDOn() {
  digitalWrite(PIN_LED_PIR, LOW);   // Turn on "motion detected" LED
}

void motionLEDOff() {
  digitalWrite(PIN_LED_PIR, HIGH);  // Turn off "motion detected" LED
}

void setModeLED(byte tColor) {
  // tColor can be MODE_ATTRACT or MODE_GAME
  if (tColor == MODE_ATTRACT) {
    digitalWrite(PIN_LED_ATTRACT_MODE, LOW);
    digitalWrite(PIN_LED_GAME_MODE, HIGH);
  } else {  // Assume tColor == MODE_GAME
  digitalWrite(PIN_LED_ATTRACT_MODE, HIGH);
  digitalWrite(PIN_LED_GAME_MODE, LOW);
  }
  return;
}

void playAudioBallyhoo(byte tMode, byte tTrack) {
  // Play ballyhoo audio, either AUDIO_STD or AUDIO_CUSTOM, track #tTrack.
  // We need to build a string of digits to send to the MP3 Trigger to play the requested track.
  // Might be best to just hard-code the string, to keep things simple.
  if (tMode == AUDIO_STD) {  // Play track #tTrack from the standard ballyhoo audio
  
  } else {  // Must be AUDIO_CUSTOM, so play track #tTrack from the custom ballyhoo audio

  }
  return;
}

void playAudioGame(byte tMode, byte tTrack) {
  // Play game audio, either AUDIO_STD or AUDIO_CUSTOM, track #tTrack.
  // We need to build a string of digits to send to the MP3 Trigger to play the requested track.
  // Might be best to just hard-code the string, to keep things simple.
  if (tMode == AUDIO_STD) {  // Play track #tTrack from the standard game audio
  
  } else {  // Must be AUDIO_CUSTOM, so play track #tTrack from the custom game audio
  
  }
  return;
}

void tapArmRelay() {
  // Momentarily close the coil on our relay which sends 18VDC to the manikin, which closes *it's* 24VDC relay.
  // A single relay tap will toggle the position of the arm, which also enables the manikin to register hits at certain times.
  // A double relay tap should only be done when the arm is in the up position, and will toggle the arm (sending it down) and
  // also will end the game -- releasing the game relay so we go back to ballyhoo mode.
  // DO NOT ALLOW THE RELAY CONTACTS TO REMAIN CLOSED FOR MORE THAN A MOMENT, OR THE MANIKIN'S 24VDC RELAY COIL COULD FRY!!!!!
  digitalWrite(PIN_ARM_TOGGLE, HIGH);   // Close relay contacts, sending 18VDC (or 24VDC) to manikin arm-toggle relay coil
  delay(150);                          // Hold contacts closed for 1/10 second
  digitalWrite(PIN_ARM_TOGGLE, LOW);  // Release relay contacts, thus releasing the manikin's 24VDC relay coil
  return;
}

void initializePinIO() {
  pinMode(PIN_GAME_VS_ATTRACT, INPUT_PULLUP);
  digitalWrite(PIN_LED_ATTRACT_MODE, HIGH);        // HIGH turns OFF this LED
  pinMode(PIN_LED_ATTRACT_MODE, OUTPUT);
  digitalWrite(PIN_LED_GAME_MODE, HIGH);           // HIGH turns OFF this LED
  pinMode(PIN_LED_GAME_MODE, OUTPUT);
  pinMode(PIN_AUDIO_STD_VS_CUSTOM, INPUT_PULLUP);
  pinMode(PIN_BALLYHOO_ON_VS_OFF, INPUT_PULLUP);
  pinMode(PIN_DIFFICULTY_EASY, INPUT_PULLUP);
  pinMode(PIN_DIFFICULTY_HARD, INPUT_PULLUP);
  pinMode(PIN_PIR_USE, INPUT_PULLUP);
  pinMode(PIN_PIR_SIGNAL, INPUT);                  // Don't need pullup since it's a signal from the PIR board
  digitalWrite(PIN_LED_PIR, HIGH);                 // HIGH turns OFF this LED
  pinMode(PIN_LED_PIR, OUTPUT);
  digitalWrite(PIN_LED_ON_BOARD, HIGH);            // HIGH turns OFF the on-board LED
  pinMode(PIN_LED_ON_BOARD, OUTPUT);
  digitalWrite(PIN_ARM_TOGGLE, LOW);               // Ensure relay coil is not energized on power-up
  pinMode(PIN_ARM_TOGGLE, OUTPUT);
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

  while (true) {
    for (int i = 1; i <= numFlashes; i++) {
      digitalWrite(PIN_LED_ON_BOARD, HIGH);
      delay(10);
      digitalWrite(PIN_LED_ON_BOARD, LOW);
      delay(100);
    }
    delay(1000);
  }
  return;  // Will never get here due to above infinite loop
}


