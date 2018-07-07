#include <Train_Consts_Global.h>
char APPVERSION[LCD_WIDTH + 1] = "A-TS1 Rev. 05/03/18";
const byte THIS_MODULE = ARDUINO_BTN;  // Not sure if/where I will use this - intended if I call a common function but will this "global" be seen there?


// *** SERIAL LCD DISPLAY CLASS:
#define _Digole_Serial_UART_                  // To tell compiler compile the serial communication only
#include <DigoleSerial.h>                     // Tell the compiler to use the DigoleSerial class library
// DigoleSerialDisp needs serial port address i.e. &Serial, &Serial1, &Serial2, or &Serial3.
// DigoleSerialDisp also needs a valid baud rate, typically 9600 or 115200.
DigoleSerialDisp digoleLCD(&Serial1, 115200); // Instantiate and name the Digole object
#include <Display_2004.h>                     // Our LCD message-display library
Display_2004 LCD(&digoleLCD);                 // Instantiate our LCD object "LCD" by passing a pointer to the Digole LCD object.
char lcdString[LCD_WIDTH + 1];                // Global array to hold strings sent to Digole 2004 LCD; last char is for null terminator.

// *** RS485 & Digital Pin MESSAGE CLASS (Inter-Arduino communications):
#include <Message_TST.h>                      // This module's communitcation class, in its .ini directory
Message_TST Message(&Serial2, 115200, &LCD);  // Instantiate our RS485/digital communications object "Message."
byte MsgIncoming[RS485_MAX_LEN];              // Global array for incoming inter-Arduino messages.  No need to init contents.
byte MsgOutgoing[RS485_MAX_LEN];              // No need to initialize contents.

void setup() {

  delay(1000);

  Serial.begin(115200);                 // PC serial monitor window (MOVE THIS CONST TO A GLOBAL ie. 9600 or 115200)
  Serial.println("Starting setup().");
  LCD.init();                           // Initialize the 20 x 04 Digole serial LCD display object "LCD."
  sprintf(lcdString, APPVERSION);       // Display the application version number on the LCD display
  LCD.send(lcdString);
  Message.Init();                       // Initialize the RS485/Digital communications object "Message."
  
  // Comment out MASTER to compile as SLAVE
  #define MASTER

  #ifdef MASTER  // Master will watch for RTX pin pulled low by slave, then send ack RS485 msg, then wait for RS485 button number msg.

  pinMode(PIN_REQ_TX_A_BTN_IN, INPUT);
  delay(1000);

  sprintf(lcdString, "Master checking...");
  LCD.send(lcdString);

  byte buttonPressed = 0;
  while (true) {

    buttonPressed = Message.GetTurnoutButtonPress();
    if (buttonPressed > 0) {  // We got a button press!
      sprintf(lcdString, "Button %2i pressed.", buttonPressed);
      LCD.send(lcdString);
      delay(5000);
    } else {
      sprintf(lcdString, "No press.");
      LCD.send(lcdString);
    }
  }

  #else  // Must be SLAVE.  Slave will RTX by pulling a pin low, then wait to receive ack from A_MAS, then send RS485 button number.

  pinMode(PIN_REQ_TX_A_BTN_OUT, OUTPUT);
  digitalWrite(PIN_REQ_TX_A_BTN_OUT, HIGH);
  delay(5000);
  sprintf(lcdString, "%.20s", "BTN sending RTX...");
  LCD.send(lcdString);
  byte buttonPressed = 17;
  Message.SendTurnoutButtonPress(buttonPressed);  // Don't even need to send it the message buffer!
  sprintf(lcdString, "Sent.");
  LCD.send(lcdString);
  

  #endif  // MASTER vs SLAVE

}

// *****************************************************************************************
// ***************************************  L O O P  ***************************************
// *****************************************************************************************

void loop() {

sprintf(lcdString, "Howdy!");
// LCD.send(lcdString);
delay(5000);
}

void endWithFlashingLED(int x) {}

void checkIfHaltPinPulledLow() {}