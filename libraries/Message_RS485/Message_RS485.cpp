// Rev: 07/16/18
// Message_RS485 handles RS485 (and *not* digital-pin) communications, via a specified serial port.

#include "Message_RS485.h"

Message_RS485::Message_RS485(HardwareSerial * t_hdwrSerial, long unsigned int t_baud, Display_2004 * t_LCD2004) {  // Constructor
  m_mySerial = t_hdwrSerial;    // Pointer to the serial port we want to use for RS485.
  m_myBaud = t_baud;            // Serial port baud rate.
  m_myLCD = t_LCD2004;          // Pointer to the LCD display for error messages.
  m_mySerial->begin(m_myBaud);  // Initialize the serial port that this object will be using.  Okay to be in constructor.
//  m_msgIncoming[RS485_LEN_OFFSET] = 0;  // Array for incoming RS485 messages.  Setting message len to zero just for fun.
//  m_msgOutgoing[RS485_LEN_OFFSET] = 0;  // Array for outgoing RS485 messages.
//  m_msgScratch[RS485_LEN_OFFSET] = 0;   // Temporarily holds message data when we don't want to clobber Incoming or Outgoing buffers.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // Put RS485 in receive mode (LOW)
  pinMode(PIN_RS485_TX_ENABLE, OUTPUT);
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  pinMode(PIN_RS485_TX_LED, OUTPUT);
  digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
  pinMode(PIN_RS485_RX_LED, OUTPUT);
 
}

bool Message_RS485::RS485GetMessage(byte t_msg[]) {
  // Returns true or false, depending if a complete message was read.
  // If this function returns true, then we are guaranteed to have a real/accurate message in the buffer, including good CRC.
  // However, this function does not check if it is to "us" (this Arduino) or not.  That's because 1) the child class will do that,
  // and because some modules snoop the messages, and act even if the message isn't addressed to them.
  // t_msg[] is also "returned" by the function since arrays are automatically passed by reference (really, by pointer.)
  // If the whole message is not available, t_msg[] will not be affected, so ok to call any time.
  // Does not require any data in the incoming RS485 serial buffer when it is called.
  // Detects incoming serial buffer overflow and fatal errors.
  // If there is a fatal error, calls endWithFlashingLED() (in calling module, so it can also invoke emergency stop if applicable.)
  // This only reads and returns one complete message at a time, regardless of how much more data may be in the incoming buffer.
  // Input byte t_msg[] is the initialized incoming byte array whose contents may be filled with a message by this function.
  byte bytesAvailableInBuffer = m_mySerial->available();  // How many bytes are waiting?  Size is 64.
  byte incomingMsgLen = m_mySerial->peek();       // First byte will be message length, or garbage is available = 0
  // bytesAvailableInBuffer must be greater than zero or there are no bytes in the incoming serial buffer.
  // bytesAvailableInBuffer must also be >= incomingMsgLen, or we don't have a complete message yet (we'll need to wait a moment.)

// ERROR JULY 28, 2017: WAS NOT CHECKING THAT BYTESAVAILABLEINBUFFER WASN'T ALSO ZERO, before checking to see if it was greater OR EQUAL TO first byte of incoming buffer (which could be garbage)

  if ((bytesAvailableInBuffer > 0) && (bytesAvailableInBuffer >= incomingMsgLen)) {  // We have at least enough bytes for a complete incoming message
    digitalWrite(PIN_RS485_RX_LED, HIGH);       // Turn on the receive LED

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "Rec'g %2i bytes", bytesAvailableInBuffer);
m_myLCD->send(lcdString);
//delay(1000);

    if (incomingMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too short!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
sprintf(lcdString, "Msg len = %3i", incomingMsgLen);
m_myLCD->send(lcdString);
sprintf(lcdString, "Avail = %3i", bytesAvailableInBuffer);
m_myLCD->send(lcdString);
      endWithFlashingLED(1);
    } else if (incomingMsgLen > RS485_MAX_LEN) {            // Message too long to be any real message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too long!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (bytesAvailableInBuffer > 60) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 in buf ovrflw!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    for (byte i = 0; i < incomingMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the t_msg[] byte array
      t_msg[i] = m_mySerial->read();
    }
//    if (t_msg[incomingMsgLen - 1] != calcChecksumCRC8(t_msg, incomingMsgLen - 1)) {   // Bad checksum.  Fatal!
    if (getChecksum(t_msg) != calcChecksumCRC8(t_msg, incomingMsgLen - 1)) {   // Bad checksum.  Fatal!

      sprintf(lcdString, "%.20s", "RS485 bad checksum!");
      m_myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "RS485 msg rec'd!");
m_myLCD->send(lcdString);
incomingMsgLen = m_mySerial->peek();     // First byte will be message length
bytesAvailableInBuffer = m_mySerial->available();  // How many bytes are waiting?  Size is 64.
sprintf(lcdString,"Now bytes avail=%3i", bytesAvailableInBuffer);

//delay(1000);

    // At this point, we have a complete and legit message with good CRC, which may or may not be for us.
    digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
    return true;
  } else {     // We don't yet have an entire message in the incoming RS485 bufffer
    return false;
  }
}

void Message_RS485::RS485SendMessage(byte t_msg[]) {
  // This routine must *only* be called when an entire message is ready to write, not a byte at a time.
  // This version, as part of the RS485 message class, automatically calculates and adds the CRC checksum.
  digitalWrite(PIN_RS485_TX_LED, HIGH);       // Turn on the transmit LED
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_TRANSMIT);  // Turn on transmit mode (set HIGH)

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "RS485 xmit mode!");
m_myLCD->send(lcdString);
//delay(1000);

  byte tMsgLen = getLen(t_msg);
  t_msg[tMsgLen - 1] = calcChecksumCRC8(t_msg, tMsgLen - 1);  // Insert the checksum into the message  ****************************** DOES THIS NEED AN ASTERISK? ******************************************
  m_mySerial->write(t_msg, tMsgLen);  // flush() makes it impossible to overflow the outgoing serial buffer, which CAN happen in my test code.
  // Although it is BLOCKING code, we'll use it at least for now.  Output buffer overflow is unpredictable without it.
  // Alternative would be to check available space in the outgoing serial buffer and stop on overflow, but how?
  m_mySerial->flush();               // wait for transmission of outgoing serial data to complete.  Takes about 0.1ms/byte.

// *** DEBUG CODE *************************************************************************************************************************************************
sprintf(lcdString, "%.20s", "RS485 xmit complete!");
m_myLCD->send(lcdString);
//delay(1000);

  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // receive mode (set LOW)
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  return;
}

byte Message_RS485::calcChecksumCRC8(const byte data[], byte len) {
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

byte Message_RS485::getLen(const byte t_msg[]) {
  return t_msg[RS485_LEN_OFFSET];
}

byte Message_RS485::getTo(const byte t_msg[]) {
  return t_msg[RS485_TO_OFFSET];
}

byte Message_RS485::getFrom(const byte t_msg[]) {
  return t_msg[RS485_FROM_OFFSET];
}

char Message_RS485::getType(const byte t_msg[]) {
  return t_msg[RS485_TYPE_OFFSET];
}

byte Message_RS485::getChecksum(const byte t_msg[]) {  // Just retrieves a byte from an existing message; does not calculate checksum!
  return t_msg[(getLen(t_msg) - 1)];
}

void Message_RS485::setLen(byte t_msg[], byte len) {
  t_msg[RS485_LEN_OFFSET] = len;
  return;
}

void Message_RS485::setTo(byte t_msg[], byte to) {
  t_msg[RS485_TO_OFFSET] = to;
  return;
}

void Message_RS485::setFrom(byte t_msg[], byte from) {
  t_msg[RS485_FROM_OFFSET] = from;
  return;
}

void Message_RS485::setType(byte t_msg[], char type) {
  t_msg[RS485_TYPE_OFFSET] = type;
  return;
}
