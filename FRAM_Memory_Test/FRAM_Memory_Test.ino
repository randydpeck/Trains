// 07/21/19: Updated to work with 256KB FRAM.
// 07/06/20: Confirmed working correctly with new FRAM, except Visual Studio complains about FRAM1CBS in line 58 and 79 - not sure what isn't right but it works.
// 10/18/16: By RDP.
// Write and then read every byte on the entire FRAM chip, ensuring that the bytes read match what was written.
// First run the test with all zoeros, and then will all ones (i.e. 255.)

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Prerequisites for Hackscribble_Ferro
//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include "Hackscribble_Ferro.h"

// 7/12/16: Modified FRAM library to change max buffer size from 64 bytes (then 96 bytes) to 128 bytes
const byte FRAM1_PIN = 53;                // Digital pin 53 is standard CS for FRAM though any pin could be used.

const unsigned int FRAM1_ROUTE_START = 128;   // Start writing FRAM1 Route Reference table data at this memory address, the lowest available address

unsigned int FRAM1BufferSize;             // We will retrieve this via a function call, but it better be 128!
unsigned long FRAM1Bottom;                // Should be 128 (address 0..127 i.e. buffer_size are reserved for any special purpose we want such as config info
unsigned long FRAM1Top;                   // Highest address we can write to...should be 8191 (addresses are 0..8191 = 8192 bytes total)

void setup() {
	
  Serial.begin(115200);

  // Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
  // Next statement creates an instance of Ferro using the standard Arduino SS pin and FRAM part number.
  // We use a customized version of the Hackscribble_Ferro library which specifies a control block length of 128 bytes.
  // Other than that, we use the standard library.  Specify the chip as MB85RS2MT (previously MB85RS64, and not MB85RS64V.)
  // You specify a FRAM part number and SS pin number like this ...
  // Could create another instance using a different SS pin number for two FRAM chips
  
  Hackscribble_Ferro FRAM1(MB85RS2MT, FRAM1_PIN);

  // Here are the possible responses from FRAM1.begin():
  //    ferroOK = 0
  //    ferroBadStartAddress = 1
  //    ferroBadNumberOfBytes = 2
  //    ferroBadFinishAddress = 3
  //    ferroArrayElementTooBig = 4
  //    ferroBadArrayIndex = 5
  //    ferroBadArrayStartAddress = 6
  //    ferroBadResponse = 7
  //    ferroPartNumberMismatch = 8
  //    ferroUnknownError = 99
  
  int fStat = FRAM1.begin();
  if (fStat != 0)  // Any non-zero result is an error
  {
    Serial.print("FRAM response not OK.  Status = ");
    Serial.println(fStat);
    while (1) {};
  }
  Serial.println("FRAM response OK.");

  FRAM1BufferSize = FRAM1.getMaxBufferSize();  // Should return 128 w/ modified library value
  const byte FRAM1CBS = FRAM1.getControlBlockSize();
  FRAM1Bottom = FRAM1.getBottomAddress();     // Should return 128 also
  FRAM1Top = FRAM1.getTopAddress();           // Should return 8191

  Serial.print("Buffer size (should be 128): ");
  Serial.println(FRAM1BufferSize);    // result = 128 (with modified library)
  Serial.print("Control block size (should be 128): ");
  Serial.println(FRAM1CBS);           // result = 128 (with modified library)
  Serial.print("Bottom (should be 128): ");
  Serial.println(FRAM1Bottom);        // result = 128
  Serial.print("Top (should be 262143 = 256K less one): ");
  Serial.println(FRAM1Top);           // result = 262143 (was 8191)


Serial.println("Press Enter to begin...");
while (Serial.available() == 0) {}
Serial.read();  

  // Define and write then read control block (first 128 bytes of FRAM):
  // FRAM1 Control Block:  Write and test all zeroes, then all ones (255s.)

  byte FRAM1ControlBuffer[FRAM1CBS];

  Serial.println("Setting control buffer in memory to all zoeres...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 0;
  }
  FRAM1.writeControlBlock(FRAM1ControlBuffer);

  Serial.println("Now reading and displaying control buffer to be sure it's all zeroes...");
  FRAM1.readControlBlock(FRAM1ControlBuffer);
  for (byte i = 0; i < FRAM1CBS; i++) {
    Serial.print(FRAM1ControlBuffer[i]); Serial.print(" ");
  }
  Serial.println();
  
  Serial.println("Scrambling control buffer array...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 170;
  }
  FRAM1.writeControlBlock(FRAM1ControlBuffer);

  Serial.println("Now reading and displaying control buffer to be sure it's all 170s...");
  FRAM1.readControlBlock(FRAM1ControlBuffer);
  for (byte i = 0; i < FRAM1CBS; i++) {
    Serial.print(FRAM1ControlBuffer[i]); Serial.print(" ");
  }
  Serial.println();

  Serial.println("Setting control buffer in memory to all zoeres...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 0;
  }
  FRAM1.writeControlBlock(FRAM1ControlBuffer);
  
  Serial.println("Now reading and displaying control buffer to be sure it's all zeroes...");
  FRAM1.readControlBlock(FRAM1ControlBuffer);
  for (byte i = 0; i < FRAM1CBS; i++) {
    Serial.print(FRAM1ControlBuffer[i]); Serial.print(" ");
  }
  Serial.println();

  Serial.println("Now setting memory buffer to all 255s...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 255;
  }
  FRAM1.writeControlBlock(FRAM1ControlBuffer);
  Serial.println("Scrambling control buffer array...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 170;
  }
  Serial.println("Now reading and displaying control buffer (should now be all 255s)...");
  FRAM1.readControlBlock(FRAM1ControlBuffer);
  for (byte i = 0; i < FRAM1CBS; i++) {
    Serial.print(FRAM1ControlBuffer[i]); Serial.print(" ");
  }
  Serial.println();

  Serial.println();
  Serial.println("End of Control Block code.");
  Serial.println();

  Serial.println("Now testing all the rest of memory, by the byte and by the 128-byte chunk...");
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 0;
  }

  // Now write code to write and then read every "regular" memory byte with zeroes and 255s.

//  Serial.println("Now filling memory will all zeroes, one byte at a time...");
  unsigned long startTime = millis();
  unsigned long stopTime;


Serial.println("Press Enter to begin...");
while (Serial.available() == 0) {}
Serial.read();  
/*
  
  byte byteToWrite[] = {0};
  
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress++) {
    FRAM1.write(FRAMAddress, 1, byteToWrite);  // (address, number_of_bytes_to_write, data    
  }
  stopTime = millis();
  Serial.print("Time to write = ");
  Serial.println(stopTime - startTime);
  
  Serial.println("Now reading every byte to confirm it is zero...");
  startTime = millis();
  byteToWrite[0] = {255};
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress++) {
    FRAM1.read(FRAMAddress, 1, byteToWrite);  // (address, number_of_bytes_to_write, data
    if (byteToWrite[0] != 0) {
      Serial.println("Write error 1.");
      while (true) { }
    }
  }
  stopTime = millis();
  Serial.print("Time to read = ");
  Serial.println(stopTime - startTime);
    
  Serial.println("Okay, every byte read was a zero.");
  Serial.println("Now filling memory with all 255s...");
  byteToWrite[0] = {255};
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress++) {
    FRAM1.write(FRAMAddress, 1, byteToWrite);  // (address, number_of_bytes_to_write, data    
  }

  Serial.println("Now reading every byte to confirm it is 255...");
  byteToWrite[0] = {0};
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress++) {
    FRAM1.read(FRAMAddress, 1, byteToWrite);  // (address, number_of_bytes_to_write, data
    if (byteToWrite[0] != 255) {
      Serial.println("Write error 2.");
      while (true) { }
    }
  }
  Serial.println("Okay, every byte read was a 255.");
  Serial.println("");

*/





 
  // Now let's write and read the entire memory in 128-byte blocks.  
  // There are 2,048 128-byte blocks in 256KB.  The first block is the control block, so
  // we can write 2,047 blocks, starting at offset 128.

  Serial.println("Now filling memory with 85's = 01010101...");
  startTime = millis();
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 85;
  }
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress=FRAMAddress+FRAM1BufferSize) {
    FRAM1.write(FRAMAddress, FRAM1BufferSize, FRAM1ControlBuffer);  // (address, number_of_bytes_to_write, data
//    Serial.print(FRAMAddress); Serial.print(" ");
  }
  Serial.println();
  
  stopTime = millis();
  Serial.print("Time to write in 128-byte blocks = ");
  Serial.println(stopTime - startTime);
  
  Serial.println("Now reading every block to confirm it is 85...");
  
Serial.println("Press Enter to begin...");
while (Serial.available() == 0) {}
Serial.read();
  
  startTime = millis();
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < FRAM1Top; FRAMAddress=FRAMAddress+FRAM1BufferSize) {
    for (byte i = 0; i < FRAM1CBS; i++) {
      FRAM1ControlBuffer[i] = 0;
    }
    FRAM1.read(FRAMAddress, FRAM1BufferSize, FRAM1ControlBuffer);  // (address, number_of_bytes_to_read, data
    for (byte i = 0; i < FRAM1CBS; i++) {
      if (FRAM1ControlBuffer[i] != 85) {
        Serial.println("Read/write error 85.");
        Serial.print("FRAMAddress = "); Serial.println(FRAMAddress);
        while (true) { }
      }
    }
//    Serial.print(FRAMAddress); Serial.print(" ");
  }
  Serial.println();
  stopTime = millis();
  Serial.print("Time to read in 128-byte blocks = ");
  Serial.println(stopTime - startTime);
  Serial.println("Okay, every byte read was an 85.");

  Serial.println("Now filling memory with all 170s = 10101010...");


Serial.println("Press Enter to begin...");
while (Serial.available() == 0) {}
Serial.read();
  
  startTime = millis();
  for (byte i = 0; i < FRAM1CBS; i++) {
    FRAM1ControlBuffer[i] = 170;
  }
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < (FRAM1Top); FRAMAddress=FRAMAddress+FRAM1BufferSize) {
    FRAM1.write(FRAMAddress, FRAM1BufferSize, FRAM1ControlBuffer);  // (address, number_of_bytes_to_write, data    
//    Serial.print(FRAMAddress); Serial.print(" ");
  }
  Serial.println();
  stopTime = millis();
  Serial.print("Time to write in 128-byte blocks = ");
  Serial.println(stopTime - startTime);
  
  Serial.println("Now reading every block to confirm it is 170...");
  
Serial.println("Press Enter to begin...");
while (Serial.available() == 0) {}
Serial.read();
  
  startTime = millis();
  for (unsigned long FRAMAddress = FRAM1Bottom; FRAMAddress < (FRAM1Top); FRAMAddress=FRAMAddress+FRAM1BufferSize) {
    for (byte i = 0; i < FRAM1CBS; i++) {
      FRAM1ControlBuffer[i] = 0;
    }
    FRAM1.read(FRAMAddress, FRAM1BufferSize, FRAM1ControlBuffer);  // (address, number_of_bytes_to_read, data
    for (unsigned int i = 0; i < FRAM1BufferSize; i++) {
      if (FRAM1ControlBuffer[i] != 170) {
        Serial.println("Write error 3.");
        while (true) { }
      }
    }
//    Serial.print(FRAMAddress); Serial.print(" ");
  }
  Serial.println();

  stopTime = millis();
  Serial.print("Time to read in 128-byte blocks = ");
  Serial.println(stopTime - startTime);
  
  Serial.println("Okay, every byte read was a 170.");
  Serial.println("");
  
  Serial.println("All tests passed!");

}

void loop() 
{

}
