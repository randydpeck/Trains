// Rev: 07/14/18
// Message_MAS is a child class of Message_RS485, and handles all RS485 and digital pin messages for A_MAS.ino.

// The Message_XXX class knows specifically which messages the calling module cares about, including the byte-level details.
// Note that Length, To, From, Type, and Checksum are all handled by methods in the parent class Message_RS485.
// Received messages that are not relevant to this module are disregarded (which in the case of A-MAS will never happen, since it's the master.)
// Messages that are returned to the calling module include the message type, as well as the relevant data as a byte array, for the caller to decypher.
// This class also provides public methods to return the relevant data fields (i.e. sensor number, triped|cleared, etc.), so the caller does not need
// to know the byte layout of the packets, only what field names it needs to extract from those packets.

// *** RS485 MESSAGE PROTOCOLS used by A-MAS.  Byte numbers represent offsets, so they start at zero. ***
// Because there are so many message types, we will document the protocols and cost offsets here.
// Note that the calling .ino program does *not* need to know byte offsets; only the names of the methods it must call to extract needed data.


// ***** ADD MESSAGE FIELD LAYOUTS FROM A-MAS HERE WHEN I GET SERIOUS ABOUT THIS CLASS *****




#ifndef MESSAGE_MAS_h
#define MESSAGE_MAS_h

#include <Message_RS485.h>
#include <Train_Consts_Global.h>

class Message_MAS : public Message_RS485 {

  public:

    Message_MAS(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * myLCD);  // Constructor

    // Note that we use the parent class, Message_RS485, for methods to get/set length, to, from, and type.  Checksum is handled automatically.
    // Here are the methods to return fields specific to this module's messages:

    void setMode(byte tMsg[], byte tMode);  // Since the message is part of the object, we probably don't need to include tMsg as a parm? *****************************************
    void setState(byte tMsg[], byte tState);  // Except maybe we do if we want separate send and receive buffers...???*************************************************************

    byte getTurnoutButtonPress();  // A_MAS: Returns 0 if A_BTN isn't asking to send us any button presses; else returns button number that was pressed.

  private:

 //   Display_2004 * myLCD;
    // Display_2004 is the name of our LCD class.
    // So create a private pointer, called myLCD, a pointer to an object of that type (class.)

    void send(byte tMsg[]);
    // Generic send.  Checksum is automatically calcualted and inserted at the end of the message.

    bool receive(byte tMsg[]);
    // Generic receive (private).  Returns true or false, depending if a complete message was read, *and* if calling module cares about the message type.
    // tmsg[] is also "returned" by the function (populated, iff there was a complete message) since arrays are passed by reference.
    // IMPORTANT: We don't want to step on the contents of the receive buffer, so tMsg[] is only modified if we receive a message that the calling module wants to see.

    byte getTurnoutButtonNum(byte tMsg[]);
    // A_MAS (private): Extracts button number from message sent by A_BTN to A_MAS with latest button press.  Internal function.

};

extern void checkIfHaltPinPulledLow();

#endif


