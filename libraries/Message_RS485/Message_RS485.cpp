// Message_RS485 is the base class that handles both RS485 and digital-pin communications.
// Rev: 04/20/18

#include <Message_RS485.h>

Message_RS485::Message_RS485(byte ID, Display_2004 * LCD2004)
{
   myModuleID = ID;
   myLCD = LCD2004;
}

bool Message_RS485::RS485GetMessage(byte tMsg[]) {
  // 10/19/16: Updated string handling for sendToLCD and Serial.print.
  // 10/1/16: Returns true or false, depending if a complete message was read.
  // If the whole message is not available, tMsg[] will not be affected, so ok to call any time.
  // Does not require any data in the incoming RS485 serial buffer when it is called.
  // Detects incoming serial buffer overflow and fatal errors.
  // If there is a fatal error, call endWithFlashingLED() which itself attempts to invoke an emergency stop.
  // This only reads and returns one complete message at a time, regardless of how much more data
  // may be waiting in the incoming RS485 serial buffer.
  // Input byte tmsg[] is the initialized incoming byte array whose contents may be filled.
  // tmsg[] is also "returned" by the function since arrays are passed by reference.
  // If this function returns true, then we are guaranteed to have a real/accurate message in the
  // buffer, including good CRC.  However, the function does not check if it is to "us" (this Arduino) or not.
  // That's because some modules snoop the messages, and act even if the message isn't addressed to them.
  byte tMsgLen = Serial2.peek();     // First byte will be message length
  byte tBufLen = Serial2.available();  // How many bytes are waiting?  Size is 64.
  if (tBufLen >= tMsgLen) {  // We have at least enough bytes for a complete incoming message
    digitalWrite(PIN_RS485_RX_LED, HIGH);       // Turn on the receive LED
    if (tMsgLen < 5) {                 // Message too short to be a legit message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too short!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tMsgLen > RS485_MAX_LEN) {            // Message too long to be any real message.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 msg too long!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    } else if (tBufLen > 60) {            // RS485 serial input buffer should never get this close to overflow.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 in buf ovrflw!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    for (int i = 0; i < tMsgLen; i++) {   // Get the RS485 incoming bytes and put them in the tMsg[] byte array
      tMsg[i] = Serial2.read();
    }
    if (tMsg[tMsgLen - 1] != calcChecksumCRC8(tMsg, tMsgLen - 1)) {   // Bad checksum.  Fatal!
      sprintf(lcdString, "%.20s", "RS485 bad checksum!");
      myLCD->send(lcdString);
      Serial.println(lcdString);
      endWithFlashingLED(1);
    }
    // At this point, we have a complete and legit message with good CRC, which may or may not be for us.
    digitalWrite(PIN_RS485_RX_LED, LOW);       // Turn off the receive LED
    return true;
  } else {     // We don't yet have an entire message in the incoming RS485 bufffer
    return false;
  }
}

void Message_RS485::RS485SendMessage(byte tMsg[]) {
  
  // 10/1/16: Updated from 9/12/16 to write entire message as single Serial2.write(msg,len) command.
  // This routine must *only* be called when an entire message is ready to write, not a byte at a time.
  // This version, as part of the RS485 message class, automatically calculates and adds the CRC checksum.
  digitalWrite(PIN_RS485_TX_LED, HIGH);       // Turn on the transmit LED
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_TRANSMIT);  // Turn on transmit mode
  byte tMsgLen = GetLen(tMsg);
  tMsg[tMsgLen - 1] = calcChecksumCRC8(tMsg, tMsgLen - 1);  // Insert the checksum into the message
  Serial2.write(tMsg, tMsgLen);  // flush() makes it impossible to overflow the outgoing serial buffer, which CAN happen in my test code.
                                 // Although it is BLOCKING code, we'll use it at least for now.  Output buffer overflow is unpredictable without it.
                                 // Alternative would be to check available space in the outgoing serial buffer and stop on overflow, but how?
  Serial2.flush();               // wait for transmission of outgoing serial data to complete.  Takes about 0.1ms/byte.
  digitalWrite(PIN_RS485_TX_ENABLE, RS485_RECEIVE);  // receive mode
  digitalWrite(PIN_RS485_TX_LED, LOW);       // Turn off the transmit LED
  return;
}

byte Message_RS485::calcChecksumCRC8(const byte data[], byte len) {

  // Rev 6/26/16
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

byte Message_RS485::GetLen(byte tMsg[]) {
  return tMsg[RS485_LEN_OFFSET];
}

byte Message_RS485::GetTo(byte tMsg[]) {
  return tMsg[RS485_TO_OFFSET];
}

byte Message_RS485::GetFrom(byte tMsg[]) {
  return tMsg[RS485_FROM_OFFSET];
}

char Message_RS485::GetType(byte tMsg[]) {
  return tMsg[RS485_TYPE_OFFSET];
}

void Message_RS485::SetLen(byte tMsg[], byte len) {
  tMsg[RS485_LEN_OFFSET] = len;
  return;
}

void Message_RS485::SetTo(byte tMsg[], byte to) {
  tMsg[RS485_TO_OFFSET] = to;
  return;
}

void Message_RS485::SetFrom(byte tMsg[], byte from) {
  tMsg[RS485_FROM_OFFSET] = from;
  return;
}

void Message_RS485::SetType(byte tMsg[], char type) {
  tMsg[RS485_TYPE_OFFSET] = type;
  return;
}
