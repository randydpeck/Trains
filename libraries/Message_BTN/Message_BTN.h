// Rev: 07/14/18
// Message_BTN is a child class of Message_RS485, and handles all RS485 and digital pin messages for A_BTN.ino.

// The Message_XXX class knows specifically which messages the calling module cares about, including the byte-level details.
// Note that Length, To, From, Type, and Checksum are all handled by methods in the parent class Message_RS485.
// Received messages that are not relevant to this module are disregarded.
// Messages that are returned to the calling module include the message type, as well as the relevant data as a byte array, for the caller to decypher.
// This class also provides public methods to return the relevant data fields (i.e. the new Mode and State), so the caller does not need to know
// the byte layout of the packets, only what field names it needs to extract from those packets.

// *** RS485 MESSAGE PROTOCOLS used by A-BTN.  Byte numbers represent offsets, so they start at zero. ***
// Because there are so many message types, we will document the protocols here and just use "magic numbers" for offsets in this class code.
// Note that the calling .ino program does *not* need to know byte offsets; only the names of the methods it must call to extract needed data.

// A-MAS BROADCAST: Mode change.  We care about mode because we only look for control panel turnout button presses while in Manual mode, Running state.
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  7
//      1  To         Byte  99 (ALL)
//      2  From       Byte  1 (A_MAS)
//      3  Msg Type   Char  'M' means this is a Mode/State update message
//      4  Mode       Byte  1..5 [Manual | Register | Auto | Park | POV]
//      5  State      Byte  1..3 [Running | Stopping | Stopped]
//      6  Cksum      Byte  0..255

// A-MAS to A-BTN: Permission for A-BTN to send which turnout button was just pressed on the control panel
// Rev: 08/31/17
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  5
//      1  To         Byte  4 (A_BTN)
//      2  From       Byte  1 (A_MAS)
//      3  Msg type   Char  'B' = Button request status (sent after it sees that a button was pressed, via a digital input from A-BTN.)
//      4  Checksum   Byte  0..255

// A-BTN to A-MAS: Sending the number of the turnout button that was just pressed on the control panel
// Rev: 08/31/17
// DIGITAL: This message requires A-BTN to send a digital RTS, then wait for an A-MAS RS485 request, before sending this packet.
// OFFSET  DESC       SIZE  CONTENTS
//      0  Length     Byte  6
//      1  To         Byte  1 (A_MAS)
//      2  From       Byte  4 (A_BTN)
//      3  Msg type   Char  'B' = Button press update
//      4  Button No. Byte  1..32
//      5  Checksum   Byte  0..255

#ifndef MESSAGE_BTN_h
#define MESSAGE_BTN_h

#include <Message_RS485.h>
#include <Train_Consts_Global.h>

class Message_BTN : public Message_RS485 {

  public:

    Message_BTN(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * myLCD);  // Constructor

    // Note that we use the parent class, Message_RS485, for methods to get/set length, to, from, and type.  Checksum is handled automatically.
    // Here are the methods to return fields specific to this module's messages:

    byte getMode(byte tMsg[]);   // Returns the 1-byte new "mode" [1..5] assuming this is a "new mode" message.
    byte getState(byte tMsg[]);  // Returns the 1-byte new "state" [1..3] assuming this is a "new mode" message.

    void sendTurnoutButtonPress(const byte button);  // Send button press message to A_MAS, including manage digital RTS signal.

  private:

  //  Display_2004 * myLCD;
    // Display_2004 is the name of our LCD class.
    // So create a private pointer, called myLCD, to an object of that type (class.)

    void send(byte tMsg[]);
    // Generic send (private).  Checksum is automatically calcualted and inserted at the end of the message.

    bool receive(byte tMsg[]);
    // Generic receive (private).  Returns true or false, depending if a complete message was read, *and* if calling module cares about the message type.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.
    // IMPORTANT: We don't want to step on the contents of the receive buffer, so tMsg[] is only modified if we receive a message that the calling module wants to see.

    void setTurnoutButtonNum(byte tMsg[], const byte button);  // Inserts button number into the appropriate byte
    // A_BTN (private): Inserts byte button number pressed into the outgoing message byte array

};

extern void checkIfHaltPinPulledLow();

#endif


