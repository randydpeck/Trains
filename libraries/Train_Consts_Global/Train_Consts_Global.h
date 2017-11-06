// Train_Consts_Global declares and defines all constants that are global to all (or nearly all) Arduino modules.
// Rev: 11/05/17

#ifndef _TRAIN_CONSTS_GLOBAL_H
#define _TRAIN_CONSTS_GLOBAL_H

#include "Arduino.h"

extern const byte LCD_WIDTH;    // Number of chars wide on the 20x04 LCD displays on the control panel

// *** ARDUINO DEVICE CONSTANTS: Here are all the different Arduinos and their "addresses" (ID numbers) for communication.
extern const byte ARDUINO_NUL;  // Use this to initialize etc.
extern const byte ARDUINO_MAS;  // Master Arduino (Main controller)
extern const byte ARDUINO_LEG;  // Output Legacy interface and accessory relay control
extern const byte ARDUINO_SNS;  // Input reads reads status of isolated track sections on layout
extern const byte ARDUINO_BTN;  // Input reads button presses by operator on control panel
extern const byte ARDUINO_SWT;  // Output throws turnout solenoids (Normal/Reverse) on layout
extern const byte ARDUINO_LED;  // Output controls the Green turnout indication LEDs on control panel
extern const byte ARDUINO_OCC;  // Output controls the Red/Green and White occupancy LEDs on control panel
extern const byte ARDUINO_ALL;  // Master broadcasting to all i.e. mode change

// *** ARDUINO PIN NUMBERS:
extern const byte PIN_FRAM1;                // Output: FRAM1 chip select.  Route Reference table and last-known-state of all trains.
extern const byte PIN_FRAM2;                // Output: FRAM2 chip select.  Event Reference and Delayed Action tables.
extern const byte PIN_FRAM3;                // Output: FRAM3 chip select.
extern const byte PIN_HALT;                 // Output: Pull low to tell A-LEG to issue Legacy Emergency Stop FE FF FF
extern const byte PIN_LED;                  // Built-in LED always pin 13
extern const byte PIN_PANEL_4_OFF;          // Input: Control panel #4 PowerMaster toggled down.  Pulled LOW.
extern const byte PIN_PANEL_4_ON;           // Input: Control panel #4 PowerMaster toggled up.  Pulled LOW.
extern const byte PIN_PANEL_BLUE_OFF;       // Input: Control panel "Blue" PowerMaster toggled down.  Pulled LOW.
extern const byte PIN_PANEL_BLUE_ON;        // Input: Control panel "Blue" PowerMaster toggled up.  Pulled LOW.
extern const byte PIN_PANEL_BROWN_OFF;      // Input: Control panel "Brown" PowerMaster toggled down.  Pulled LOW.
extern const byte PIN_PANEL_BROWN_ON;       // Input: Control panel "Brown" PowerMaster toggled up.  Pulled LOW.
extern const byte PIN_PANEL_RED_OFF;        // Input: Control panel "Red" PowerMaster toggled down.  Pulled LOW.
extern const byte PIN_PANEL_RED_ON;         // Input: Control panel "Red" PowerMaster toggled up.  Pulled LOW.
extern const byte PIN_REQ_TX_A_BTN_OUT;     // A_BTN output pin pulled LOW when it wants to tell A-MAS a turnout button has been pressed.
extern const byte PIN_REQ_TX_A_BTN_IN;      // A_MAS input pin pulled LOW by A-BTN when it wants to tell A-MAS a turnout button has been pressed.
extern const byte PIN_REQ_TX_A_LEG_OUT;     // A_LEG output pin pulled LOW when it wants to send A_MAS a message.
extern const byte PIN_REQ_TX_A_LEG_IN;      // A_MAS input pin pulled LOW by A_LEG when it wants to send A_MAS a message.
extern const byte PIN_REQ_TX_A_SNS_OUT;     // A_SNS output pin pulled LOW when it wants to send A_MAS an occupancy sensor change message.
extern const byte PIN_REQ_TX_A_SNS_IN;      // A_MAS input pin pulled LOW by A_SNS when it wants to send A_MAS an occupancy sensor change message.
extern const byte PIN_ROTARY_1;             // Input: Rotary Encoder pin 1 of 2 (plus Select)
extern const byte PIN_ROTARY_2;             // Input: Rotary Encoder pin 2 of 2 (plus Select)
extern const byte PIN_ROTARY_AUTO;          // Input: Rotary mode "Auto."  Pulled LOW
extern const byte PIN_ROTARY_LED_AUTO;      // Output: Rotary Auto position LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_MANUAL;    // Output: Rotary Manual position LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_PARK;      // Output: Rotary Park position LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_POV;       // Output: Rotary P.O.V. position LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_REGISTER;  // Output: Rotary Register position LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_START;     // Output: Rotary Start button internal LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_LED_STOP;      // Output: Rotary Stop button internal LED.  Pull LOW to turn on.
extern const byte PIN_ROTARY_MANUAL;        // Input: Rotary mode "Manual."  Pulled LOW.
extern const byte PIN_ROTARY_PARK;          // Input: Rotary mode "Park."  Pulled LOW
extern const byte PIN_ROTARY_POV;           // Input: Rotary mode "P.O.V."  Pulled LOW
extern const byte PIN_ROTARY_PUSH;          // Input: Rotary Encoder "Select" (pushbutton) pin
extern const byte PIN_ROTARY_REGISTER;      // Input: Rotary mode "Register."  Pulled LOW.
extern const byte PIN_ROTARY_START;         // Input: Rotary "Start" button.  Pulled LOW
extern const byte PIN_ROTARY_STOP;          // Input: Rotary "Stop" button.  Pulled LOW
extern const byte PIN_RS485_RX_LED;         // Output: set HIGH to turn on YELLOW when RS485 is RECEIVING data
extern const byte PIN_RS485_TX_ENABLE;      // Output: set HIGH when in RS485 transmit mode, LOW when not transmitting
extern const byte PIN_RS485_TX_LED;         // Output: set HIGH to turn on BLUE LED when RS485 is TRANSMITTING data
extern const byte PIN_SPEAKER;              // Output: Piezo buzzer connects positive here
extern const byte PIN_WAV_TRIGGER;          // Input: LOW when a track is playing, else HIGH.
 
// *** OPERATING MODES AND STATES:
extern const byte MODE_UNDEFINED;
extern const byte MODE_MANUAL;
extern const byte MODE_REGISTER;
extern const byte MODE_AUTO;
extern const byte MODE_PARK;
extern const byte MODE_POV;
extern const byte STATE_UNDEFINED;
extern const byte STATE_RUNNING;
extern const byte STATE_STOPPING;
extern const byte STATE_STOPPED;

#endif

