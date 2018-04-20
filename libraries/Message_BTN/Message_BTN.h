// Rev: 04/20/18
// Message_BTN is a child class of Message_RS485.

// *** RS485 MESSAGE PROTOCOLS used by A-BTN.  Byte numbers represent offsets, so they start at zero. ***

// A-MAS BROADCAST: Mode change.  We care about mode because we only look for control panel turnout button presses while in Manual mode, Running state.
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

#ifndef _MESSAGE_BTN_h
#define _MESSAGE_BTN_h

#include <Message_RS485.h>
#include <Train_Consts_Global.h>

const byte RS485_BUTTON_NO_OFFSET = 4;    // Offset into RS485 message where button no. that was pressed is stored

class Message_BTN : public Message_RS485 {

  public:

    // Constructor:
    Message_BTN(Display_2004 * LCD2004);

    // Receive returns true or false, depending if a complete message was read.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.
    bool Receive(byte tMsg[]);

    // Checksum is automatically calcualted and inserted at the end of the message
    void Send(byte tMsg[]);

    // Function unique to A_BTN, inserts byte button number pressed into the outgoing message byte array
    void SetButtonNo(byte tMsg[], byte button);  // Inserts button number into the appropriate byte

  private:

};

#endif


