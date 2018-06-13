// Rev: 05/06/18
// Network_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

#include "Network_RS485.h"

Network_RS485::Network_RS485(HardwareSerial * hdwrSerial, long unsigned int baud, Display_2004 * LCD2004) {  // Constructor
  m_Serial = hdwrSerial;  // Pointer to the serial port we want to use for RS485 (*not* the serial monitor!)
  m_Baud = baud;          // Serial port baud rate
  m_LCD = LCD2004;        // Pointer to the LCD display for error messages
}

void Network_RS485::InitPort() {
  // Initialize the serial port that this object will be using...

  m_Serial->begin(m_Baud);

}

// bool Network_RS485::RS485GetMessage(byte tMsg[]) {
bool Network_RS485::RS485GetMessage() {
  // Returns true or false, depending if a complete message was read.
  // If this function returns true, then we are guaranteed to have a real/accurate message in the buffer, including good CRC.
  // However, this function does not check if it is to "us" (this Arduino) or not.  That's because 1) the child class will do that,
  // and because some modules snoop the messages, and act even if the message isn't addressed to them.
  // tmsg[] is also "returned" by the function since arrays are automatically passed by reference.
  // If the whole message is not available, tMsg[] will not be affected, so ok to call any time.
  // Does not require any data in the incoming RS485 serial buffer when it is called.
  // Detects incoming serial buffer overflow and fatal errors.
  // If there is a fatal error, calls endWithFlashingLED() (in calling module, so it can also invoke emergency stop if applicable.)
  // This only reads and returns one complete message at a time, regardless of how much more data may be in the incoming buffer.
  // Input byte tmsg[] is the initialized incoming byte array whose contents may be filled.
  
  byte tMsgLen = m_Serial->peek();     // First byte will be message length
  byte tBufLen = m_Serial->available();  // How many bytes are waiting?  Size is 64.

  if (tBufLen >= tMsgLen) {  // We have at least enough bytes for a complete incoming message
    digitalWrite(PIN_RS485_RX_LED, HIGH);       // Turn on the receive LED
    if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too short!");
      m_LCD->send(lcdString);
      endWithFlashingLED(1);
    } else if (tMsgLen > RS485_MAX_LEN) {            // Message too long to be any real message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too long!");
      m_LCD->send(lcdString);
      endWithFlashingLED(1);
    } else if (tBufLen > 60) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 in buf ovrflw!");
      m_LCD->send(lcdString);
      endWithFlashingLED(1);
    }
    for (int i = 0; i < tMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the tMsg[] byte array
      MsgBufIn[i] = m_Serial->read();
    }
    if (MsgBufIn[tMsgLen - 1] != calcChecksumCRC8(MsgBufIn, tMsgLen - 1)) {   // Bad checksum.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 bad checksum!");
      m_LCD->send(lcdString);
      endWithFlashingLED(1);
    } else {
      sprintf(lcdString, "%.20s", "RS485 GOOD checksum!");
      m_LCD->send(lcdString);
    }
    // At this point, we have a complete and legit message with good CRC, which may or may not be for us.
    digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
    return true;
  } else {     // We don't yet have an entire message in the incoming RS485 bufffer
    return false;
  }
}

void Network_RS485::RS485SendMessage() {
  // This routine must *only* be called when an entire message is ready to write, not a byte at a time.
  // This version, as part of the RS485 message class, automatically calculates and adds the CRC checksum.
  digitalWrite(PIN_RS485_TX_LED, HIGH);       // Turn on the transmit LED
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_TRANSMIT);  // Turn on transmit mode
  byte tMsgLen = MsgBufOut[RS485_LEN_OFFSET];  // GetLen(); no good because it works on incoming buffer, not outgoing
  MsgBufOut[tMsgLen - 1] = calcChecksumCRC8(MsgBufOut, tMsgLen - 1);  // Insert the checksum into the message
  m_Serial->write(MsgBufOut, tMsgLen);  // flush() makes it impossible to overflow the outgoing serial buffer, which CAN happen in my test code.
                                   // Although it is BLOCKING code, we'll use it at least for now.  Output buffer overflow is unpredictable without it.
                                   // Alternative would be to check available space in the outgoing serial buffer and stop on overflow, but how?
  m_Serial->flush();               // wait for transmission of outgoing serial data to complete.  Takes about 0.1ms/byte.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // receive mode
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  return;
}

byte Network_RS485::calcChecksumCRC8(const byte data[], byte len) {
  // calcChecksumCRC8 returns the CRC-8 checksum to use or confirm.
  // Used for RS485 messages
  // Sample call: msg[msgLen - 1] = calcChecksumCRC8(msg, msgLen - 1);
  // We will send (sizeof(msg) - 1) and make the LAST byte
  // the CRC byte - so not calculated as part of itself ;-)
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}

void Network_RS485::ClearIncomingMessage() {
  // Reset fields of the incoming message buffer.  Useful before calling RS485GetMessage().
  MsgBufOut[RS485_LEN_OFFSET] = 0;
  MsgBufOut[RS485_TO_OFFSET] = ARDUINO_NUL;
  MsgBufOut[RS485_FROM_OFFSET] = ARDUINO_NUL;
  MsgBufOut[RS485_TYPE_OFFSET] = ' ';
  return;
}

byte Network_RS485::GetLen() {
  return MsgBufIn[RS485_LEN_OFFSET];
}

byte Network_RS485::GetTo() {
  return MsgBufIn[RS485_TO_OFFSET];
}

byte Network_RS485::GetFrom() {
  return MsgBufIn[RS485_FROM_OFFSET];
}

char Network_RS485::GetType() {
  return MsgBufIn[RS485_TYPE_OFFSET];
}

void Network_RS485::SetLen(byte len) {
  MsgBufOut[RS485_LEN_OFFSET] = len;
  return;
}

void Network_RS485::SetTo(byte to) {
  MsgBufOut[RS485_TO_OFFSET] = to;
  return;
}

void Network_RS485::SetFrom(byte from) {
  MsgBufOut[RS485_FROM_OFFSET] = from;
  return;
}

void Network_RS485::SetType(char type) {
  MsgBufOut[RS485_TYPE_OFFSET] = type;
  return;
}
