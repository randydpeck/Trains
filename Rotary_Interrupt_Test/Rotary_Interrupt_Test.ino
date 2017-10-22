// Rotary encoder test code tripped by interrupts.  Rev: 10/25/2016 by RDP
// This code has a blocking delay for debouncing the pushbutton.
// If I add the R-C/Schmitt Trigger hardware debouncer, I can remove this delay.
// We do *NOT* need the rotary library for this to work.
// Here is a cross reference of the Mega interrupt number to pin numbers.  This is handled
// automatically via the "digitalPinToInterrupt() function.
// 
// Mega2560	int.0	int.1	int.2	int.3	int.4	int.5
// Digital Pin	2	3	21	20	19	18
//
// Since this code will probably only be used in a specific part of A-OCC, at least where
// the pushbutton part of the encoder is needed, then it might make sense to disable
// interrupts for all of the rest of the code -- whenever we don't care about the status
// of the rotary encoder.  Because otherwise we will be calling the two interrupt service
// routines when they aren't even needed.  On the other hand, it's not likely that the
// operator would be turning the rotary dial or pressing the button unless there was a
// reason to be doing so, and just having the interrupts enabled but not being called is
// probably not much of a burden to the CPU.

// Why not just set rotaryTurned = false at the beginning of the block of code that might look for it?
// Ditto with rotaryPushed.

// IMPORTANT: Modify this code in A-OCC so that when I am not specifically looking for a button press, that
// we have a flag set and the ISR for button press will immediately return without doing anything.
// This is so that the VERY slow button press ISR will not be allowed to run at any time that we might
// possibly expect to receive a serial message via RS485.  We will ONLY allow the button press ISR to run
// when we are in Register mode, and A-MAS will be waiting to receive serial data from A-OCC.
// HOPEFULLY, the rotation ISR is fast enough that it will not interfere with incoming serial data if
// the operator is spinning the dial when a serial message arrives (which could easily happen.)
// If we see the system halt due to a bad CRC, then we should first consider if the operator was turning
// the rotary encoder dial at the time the message was incoming.  In which case, since the USART can only
// received TWO BYTES when an ISR is running, we probably had an incoming USART buffer overflow.
// There is a way to check for this in code, by looking at the UCSR0A register bit 3 (DOR0 - data overrun,)
// but I couldn't figure out how to do it and decided it wasn't worth the effort at this point since the
// CRC check will catch any errors regardless, and I will suspect the buffer overflow problem described here.

// Arduino pins the encoder is attached to. Attach the center to ground.
#define ROTARY_PIN1 2   // Plus one of the two rotary encoder outside pins into Mega pin 2
#define ROTARY_PIN2 3   // and the other into pin 3 (the middle pin is ground.)
#define ROTARY_PUSH 19  // Pushbutton pin goes to 19 because that's also an interrupt on Mega
#define DIR_CCW 0x10    // decimal 16
#define DIR_CW 0x20     // decimal 32

// This full-step state table emits a code at 00 only
const unsigned char ttable[7][4] = {
  {0x0, 0x2, 0x4,  0x0}, {0x3, 0x0, 0x1, 0x10},
  {0x3, 0x2, 0x0,  0x0}, {0x3, 0x2, 0x1,  0x0},
  {0x6, 0x0, 0x4,  0x0}, {0x6, 0x5, 0x0, 0x20},
  {0x6, 0x5, 0x4,  0x0},
};

volatile unsigned char rotaryNewState = 0;     // Value will be 0, 16, or 32 depending on how rotary being turned
volatile unsigned char rotaryTurned = false;   // Flag set by ISR so we know to process in loop()
volatile unsigned char rotaryPushed = false;   // Flag set by ISR so we know to process in loop()

void setup() {

  Serial.begin(115200);

  pinMode(ROTARY_PIN1, INPUT_PULLUP);
  pinMode(ROTARY_PIN2, INPUT_PULLUP);
  pinMode(ROTARY_PUSH, INPUT_PULLUP);
}

void loop() {

  Serial.println("Interrupts are not enabled now...");

  long waitTime = millis();
  do {
    if (rotaryTurned) {   // we have a new, non-zero state
      Serial.println(rotaryNewState == DIR_CCW ? "LEFT" : "RIGHT");
      rotaryTurned = false;  // reset
      rotaryNewState = 0;    // back to neither CW nor CCW
    }

    if (rotaryPushed) {
      Serial.println("Rotary button has been pressed.");
      rotaryPushed = false;  // reset
    }
  } while ((millis() - waitTime) < 5000);    // Wait 5 seconds

  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN1), ISR_rotary_turn, CHANGE);  // Interrupt 0 is Mega pin 2
  attachInterrupt(digitalPinToInterrupt(ROTARY_PIN2), ISR_rotary_turn, CHANGE);  // Interrupt 1 is Mega pin 3
  attachInterrupt(digitalPinToInterrupt(ROTARY_PUSH), ISR_rotary_push, FALLING); // Interrupt 4 is Mega pin 19

  Serial.println("Interrupts are enabled...");
  waitTime = millis();
  do {
    if (rotaryTurned) {   // we have a new, non-zero state
      Serial.println(rotaryNewState == DIR_CCW ? "LEFT" : "RIGHT");
      rotaryTurned = false;  // reset
      rotaryNewState = 0;    // back to neither CW nor CCW
    }

    if (rotaryPushed) {
      Serial.println("Rotary button has been pressed.");
      rotaryPushed = false;  // reset
    }
  } while ((millis() - waitTime) < 5000);

  detachInterrupt(digitalPinToInterrupt(ROTARY_PIN1));  // Interrupt 0 is Mega pin 2
  detachInterrupt(digitalPinToInterrupt(ROTARY_PIN2));  // Interrupt 1 is Mega pin 3
  detachInterrupt(digitalPinToInterrupt(ROTARY_PUSH)); // Interrupt 4 is Mega pin 19
  
}

// *****************************************************************************************
// ************************ F U N C T I O N   D E F I N I T I O N S ************************
// *****************************************************************************************

// *****************************************************************************************
// *** FIRST WE DEFINE FUNCTIONS UNIQUE TO THIS ARDUINO (not shared by all in any event) ***
// *****************************************************************************************

// *****************************************************************************************
// ******************************* ROTARY ENCODER INTERRUPTS *******************************
// *****************************************************************************************

// Rev: 10/26/16.  Interrupt-driven rotary encoder turns and button press functions.
// We do *NOT* need a rotary encoder library for this to work.
// Here are the Arduino pins the encoder must be connected to. Attach the center to ground:
// #define ROTARY_PIN1 2   // Plus one of the two rotary encoder outside pins into Mega pin 2
// #define ROTARY_PIN2 3   // and the other into pin 3 (the middle pin is ground.)
// #define ROTARY_PUSH 19  // Pushbutton pin goes to 19 because that's also an interrupt on Mega
// #define DIR_CCW 0x10    // decimal 16
// #define DIR_CW 0x20     // decimal 32
//
// Here is a cross reference of the Mega interrupt number to pin numbers.  This is handled automatically via the
// digitalPinToInterrupt() function:
//   Mega2560  int.0 int.1 int.2 int.3 int.4 int.5
//   Digital Pin  2 3 21  20  19  18
//
// Requires a global table and some global variables to be defined above Setup():
// This full-step state table emits a code at 00 only:
//   const unsigned char ttable[7][4] = {
//     {0x0, 0x2, 0x4,  0x0}, {0x3, 0x0, 0x1, 0x10},
//     {0x3, 0x2, 0x0,  0x0}, {0x3, 0x2, 0x1,  0x0},
//     {0x6, 0x0, 0x4,  0x0}, {0x6, 0x5, 0x0, 0x20},
//     {0x6, 0x5, 0x4,  0x0}
//   };
//
// Requires a few global volatile variables:
//   volatile unsigned char rotaryNewState = 0;     // Value will be 0, 16, or 32 depending on how rotary being turned
//   volatile unsigned char rotaryTurned = false;   // Flag set by ISR so we know to process in loop()
//   volatile unsigned char rotaryPushed = false;   // Flag set by ISR so we know to process in loop()
//
// Requires attachInterrupt statements in setup():
//   attachInterrupt(digitalPinToInterrupt(ROTARY_PIN1), ISR_rotary_turn, CHANGE);  // Interrupt 0 is Mega pin 2
//   attachInterrupt(digitalPinToInterrupt(ROTARY_PIN2), ISR_rotary_turn, CHANGE);  // Interrupt 1 is Mega pin 3
//   attachInterrupt(digitalPinToInterrupt(ROTARY_PUSH), ISR_rotary_push, FALLING); // Interrupt 4 is Mega pin 19

void ISR_rotary_push() {
  // Rev: 10/26/16.  Set rotaryPushed = true, after waiting for bounce to settle.
  // We can eliminate delay if we implement hardware debounce, but it only takes 5 milliseconds, and we are only
  // delayed when the operator presses the button, so this is nothing.  Just keep software debounce.
  delayMicroseconds(5000);   // 1000 microseconds is not quite enough to totally eliminate bounce.
  if (digitalRead(ROTARY_PUSH) == LOW) {
    rotaryPushed = true;
  }
}  

void ISR_rotary_turn() {
  // Rev: 10/26/16. Called when rotary encoder turned either direction, returns rotaryTurned and rotaryNewState globals.
  static unsigned char state = 0;
  unsigned char pinState = (digitalRead(ROTARY_PIN2) << 1) | digitalRead(ROTARY_PIN1);
  state = ttable[state & 0xf][pinState];
  unsigned char stateReturn = (state & 0x30);
  if ((stateReturn == 16) || (stateReturn == 32)) {
    rotaryTurned = true;
    rotaryNewState = stateReturn;
  }
}

