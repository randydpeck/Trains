// Rev: 05/05/18
// Train_Consts_Global declares and defines all constants that are global to all (or nearly all) Arduino modules.

// Header (.h) files should contain:
// * Header guards #ifndef/#define/#endif
// * Source code documentation i.e. purpose of parms, and return values of functions.
// * Type and constant DEFINITIONS.  Note this does not consume memory if not used.
// * External variable, structure, function, and object DECLARATIONS
// Large numbers of structure declarations should be split into separate headers.
// Struct declarations do not take up any memory, so no harm done if they get included with code that never defines them.
// Structure definitions can be in a corresponding .cpp file, *or* in the main source file (probably better for me.)
// enum and constants should be DEFINED not just declared in the .h file.

#ifndef TRAIN_CONSTS_GLOBAL_H
#define TRAIN_CONSTS_GLOBAL_H

#include <Arduino.h>  // Allows use of "byte" and other Arduino-specifics

// Even if we decide to define the following three non-const arrays in the accompanying .cpp file,  I don't think the following
// extern statements are needed, because it seems to compile file without them...
// They are defined in the .cpp file.
// extern       byte RS485MsgIncoming[];  // No need to initialize contents
// extern       byte RS485MsgOutgoing[];
// extern       char lcdString[];         // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

extern const byte THIS_MODULE;            // Will be set to A_MAS, A_SWT, etc. for each module

const byte LCD_WIDTH = 20;                // 2004 (20 char by 4 lines) LCD display

const byte TOTAL_TURNOUTS    =  32;       // 30 connected, but 32 relays.

// Note that the serial input buffer is only 64 bytes, which means that we need to keep emptying it since there
// will be many commands between Arduinos, even though most may not be for THIS Arduino.  If the buffer overflows,
// then we will be totally screwed up (but it will be apparent in the checksum.)
const byte RS485_MAX_LEN     = 20;        // buffer length to hold the longest possible RS485 message.  Just a guess.
const byte RS485_LEN_OFFSET  =  0;        // first byte of message is always total message length in bytes
const byte RS485_TO_OFFSET   =  1;        // second byte of message is the ID of the Arduino the message is addressed to
const byte RS485_FROM_OFFSET =  2;        // third byte of message is the ID of the Arduino the message is coming from
const byte RS485_TYPE_OFFSET =  3;        // fourth byte of message is the type of message such as M for Mode, S for Smoke, etc.
// Note also that the LAST byte of the message is a CRC8 checksum of all bytes except the last
const byte RS485_TRANSMIT    = HIGH;      // HIGH = 0x1.  How to set TX_CONTROL pin when we want to transmit RS485
const byte RS485_RECEIVE     = LOW;       // LOW = 0x0.  How to set TX_CONTROL pin when we want to receive (or NOT transmit) RS485

const byte RS485_BTN_BUTTON_NO_OFFSET = 4;      // Offset into "real" RS485 message where button no. that was pressed is stored (A_MAS and A_BTN)

// *** ARDUINO DEVICE CONSTANTS: Here are all the different Arduinos and their "addresses" (ID numbers) for communication.
const byte ARDUINO_NUL =  0;              // Use this to initialize etc.
const byte ARDUINO_MAS =  1;              // Master Arduino (Main controller)
const byte ARDUINO_LEG =  2;              // Output Legacy interface and accessory relay control
const byte ARDUINO_SNS =  3;              // Input reads reads status of isolated track sections on layout
const byte ARDUINO_BTN =  4;              // Input reads button presses by operator on control panel
const byte ARDUINO_SWT =  5;              // Output throws turnout solenoids (Normal/Reverse) on layout
const byte ARDUINO_LED =  6;              // Output controls the Green turnout indication LEDs on control panel
const byte ARDUINO_OCC =  7;              // Output controls the Red/Green and White occupancy LEDs on control panel
const byte ARDUINO_ALL = 99;              // Master broadcasting to all i.e. mode change

// *** ARDUINO PIN NUMBERS:
const byte PIN_FRAM1               = 11;  // Output: FRAM1 chip select.  Route Reference table and last-known-state of all trains.
const byte PIN_FRAM2               = 12;  // Output: FRAM2 chip select.  Event Reference and Delayed Action tables.
const byte PIN_FRAM3               = 19;  // Output: FRAM3 chip select.
const byte PIN_HALT                =  9;  // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF
const byte PIN_LED                 = 13;  // Built-in LED always pin 13
const byte PIN_PANEL_4_OFF         = 37;  // Input: Control panel #4 PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_4_ON          = 35;  // Input: Control panel #4 PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_BLUE_OFF      = 29;  // Input: Control panel "Blue" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_BLUE_ON       = 27;  // Input: Control panel "Blue" PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_BROWN_OFF     = 25;  // Input: Control panel "Brown" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_BROWN_ON      = 23;  // Input: Control panel "Brown" PowerMaster toggled up.  Pulled LOW.
const byte PIN_PANEL_RED_OFF       = 33;  // Input: Control panel "Red" PowerMaster toggled down.  Pulled LOW.
const byte PIN_PANEL_RED_ON        = 31;  // Input: Control panel "Red" PowerMaster toggled up.  Pulled LOW.
const byte PIN_REQ_TX_A_BTN_OUT    =  8;  // A_BTN output pin pulled LOW when it wants to tell A-MAS a turnout button has been pressed.
const byte PIN_REQ_TX_A_BTN_IN     =  8;  // A_MAS input pin pulled LOW by A-BTN when it wants to tell A-MAS a turnout button has been pressed.
const byte PIN_REQ_TX_A_LEG_OUT    =  8;  // A_LEG output pin pulled LOW when it wants to send A_MAS a message.
const byte PIN_REQ_TX_A_LEG_IN     =  2;  // A_MAS input pin pulled LOW by A_LEG when it wants to send A_MAS a message.
const byte PIN_REQ_TX_A_SNS_OUT    =  8;  // A_SNS output pin pulled LOW when it wants to send A_MAS an occupancy sensor change message.
const byte PIN_REQ_TX_A_SNS_IN     =  3;  // A_MAS input pin pulled LOW by A_SNS when it wants to send A_MAS an occupancy sensor change message.
const byte PIN_ROTARY_1            =  2;  // Input: Rotary Encoder pin 1 of 2 (plus Select)
const byte PIN_ROTARY_2            =  3;  // Input: Rotary Encoder pin 2 of 2 (plus Select)
const byte PIN_ROTARY_AUTO         = 27;  // Input: Rotary mode "Auto."  Pulled LOW
const byte PIN_ROTARY_LED_AUTO     = 45;  // Output: Rotary Auto position LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_MANUAL   = 41;  // Output: Rotary Manual position LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_PARK     = 47;  // Output: Rotary Park position LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_POV      = 49;  // Output: Rotary P.O.V. position LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_REGISTER = 43;  // Output: Rotary Register position LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_START    = 37;  // Output: Rotary Start button internal LED.  Pull LOW to turn on.
const byte PIN_ROTARY_LED_STOP     = 39;  // Output: Rotary Stop button internal LED.  Pull LOW to turn on.
const byte PIN_ROTARY_MANUAL       = 23;  // Input: Rotary mode "Manual."  Pulled LOW.
const byte PIN_ROTARY_PARK         = 29;  // Input: Rotary mode "Park."  Pulled LOW
const byte PIN_ROTARY_POV          = 31;  // Input: Rotary mode "P.O.V."  Pulled LOW
const byte PIN_ROTARY_PUSH         = 19;  // Input: Rotary Encoder "Select" (pushbutton) pin
const byte PIN_ROTARY_REGISTER     = 25;  // Input: Rotary mode "Register."  Pulled LOW.
const byte PIN_ROTARY_START        = 33;  // Input: Rotary "Start" button.  Pulled LOW
const byte PIN_ROTARY_STOP         = 35;  // Input: Rotary "Stop" button.  Pulled LOW
const byte PIN_RS485_RX_LED        =  6;  // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
const byte PIN_RS485_TX_ENABLE     =  4;  // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
const byte PIN_RS485_TX_LED        =  5;  // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
const byte PIN_SPEAKER             =  7;  // Output: Piezo buzzer connects positive here
const byte PIN_WAV_TRIGGER         = 10;  // Input: LOW when a track is playing, else HIGH.

// *** OPERATING MODES AND STATES:
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

#endif

