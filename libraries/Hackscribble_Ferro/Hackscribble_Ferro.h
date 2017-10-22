/*

        10/17/16: RDP modified from default 0x40 to 0x80 by Randy RDP on 10/17/16 for 128-byte control block.  Affects one line only, below.

	Hackscribble_Ferro Library
	==========================
	
	Connects Fujitsu Ferroelectric RAM (MB85RS range) to your 
	Arduino to add up to 256KB of fast, non-volatile storage.
	
	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".
	
	
	Created on 18 April 2014
	By Ray Benitez
	Last modified on 29 September 2014
	By Ray Benitez
	Change history in "README.md"
		
	This software is licensed by Ray Benitez under the MIT License.
	
	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble

*/

#ifndef HACKSCRIBBLE_FERRO_H
#define HACKSCRIBBLE_FERRO_H

#include "Arduino.h"
#include <SPI.h>


#define HS_SPI_DEFAULT_MODE			SPI_MODE0
#define HS_SPI_DEFAULT_CLOCK		SPI_CLOCK_DIV2


// MB85RS part numbers

enum ferroPartNumber
{
	MB85RS16 = 0,		// 2KB
	MB85RS64,			// 8KB
	MB85RS128A,			// 16KB older model
	MB85RS128B,			// 16KB newer model
	MB85RS256A,			// 32KB older model
	MB85RS256B,			// 32KB newer model
	MB85RS1MT,			// 128KB
	MB85RS2MT,			// 256KB
	numberOfPartNumbers
};
		
// MB85RS address lengths

enum ferroAddressLength
{
	ADDRESS16BIT = 0,
	ADDRESS24BIT,
	numberOfAddressLengths
};

// Result codes

enum ferroResult
{
	ferroOK = 0,
	ferroBadStartAddress,
	ferroBadNumberOfBytes,
	ferroBadFinishAddress,
	ferroArrayElementTooBig,
	ferroBadArrayIndex,
	ferroBadArrayStartAddress,
	ferroBadResponse,
	ferroPartNumberMismatch,
	ferroUnknownError = 99
};


class Hackscribble_Ferro
{
private:

	ferroPartNumber _partNumber;
	byte _chipSelect;
	volatile uint8_t *_out;
	uint8_t _bit;
	static boolean _spiIsRunning;
	
	// FRAM opcodes
	static const byte _WREN = 0x06;
	static const byte _WRDI = 0x04;
	static const byte _WRITE = 0x02;
	static const byte _READ = 0x03;
	static const byte _RDSR = 0x05;
	static const byte _WRSR = 0x01;
	static const byte _RDID = 0x9F;

	// Dummy write value for SPI read
	static const byte _dummy = 0x00;

	// Set maximum size of buffer used to write to and read from FRAM
	// Do not exceed 0x80 to prevent problems with maximum size structs in FerroArray
	static const byte _maxBufferSize = 0x80;   // This modified from default 0x40 to 0x80 by Randy RDP on 10/17/16 for 128-byte control block

	// Used in constructor to set size of usable FRAM memory, reserving some bytes as a control block
	unsigned long _topAddressForPartNumber[numberOfPartNumbers];
	ferroAddressLength _addressLengthForPartNumber[numberOfPartNumbers];
	ferroAddressLength _addressLength;
	byte _densityCode[numberOfPartNumbers];
	unsigned long _baseAddress;
	unsigned long _bottomAddress;
	unsigned long _topAddress;
	unsigned long _numberOfBuffers;
	
	// FRAM current next byte to allocate
	unsigned long _nextFreeByte;

	uint8_t _readStatusRegister(void);
	void _writeStatusRegister(uint8_t value);
	void _readMemory(unsigned long address, uint8_t numberOfBytes, uint8_t *buffer);
	void _writeMemory(unsigned long address, uint8_t numberOfBytes, uint8_t *buffer);
	void _initialiseSPI(void);
	void _initialiseCS(void);
	void _select();
	void _deselect();

public:

	Hackscribble_Ferro(ferroPartNumber partNumber = MB85RS128A, byte chipSelect = SS);
	ferroResult begin();
	ferroPartNumber getPartNumber();
	byte readProductID();
	byte getMaxBufferSize();
	unsigned long getBottomAddress();
	unsigned long getTopAddress();
	ferroResult checkForFRAM();
	byte getControlBlockSize();
	void writeControlBlock(byte *buffer);
	void readControlBlock(byte *buffer);
	ferroResult read(unsigned long startAddress, byte numberOfBytes, byte *buffer);
	ferroResult write(unsigned long startAddress, byte numberOfBytes, byte *buffer);
	unsigned long allocateMemory(unsigned long numberOfBytes, ferroResult& result);
	ferroResult format();

};

/*
class Hackscribble_FerroArray
{
private:

	unsigned long _numberOfElements;
	byte _sizeOfElement;
	unsigned long _startAddress;
	Hackscribble_Ferro& _f;
	
public:
	
	Hackscribble_FerroArray(Hackscribble_Ferro& f, unsigned long numberOfElements, byte sizeOfElement, ferroResult &result);
	void readElement(unsigned long index, byte *buffer, ferroResult &result);
	void writeElement(unsigned long index, byte *buffer, ferroResult &result);
	unsigned long getStartAddress();
	
};
*/

#endif
