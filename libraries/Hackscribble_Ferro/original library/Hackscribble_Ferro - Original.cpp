/*

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

 
#include "wiring_digital.c"
#include "Arduino.h"
#include <Hackscribble_Ferro.h>


boolean Hackscribble_Ferro::_spiIsRunning = false;


Hackscribble_Ferro::Hackscribble_Ferro(ferroPartNumber partNumber, byte chipSelect): _partNumber(partNumber), _chipSelect(chipSelect)
{
	_topAddressForPartNumber[MB85RS16]		= 0x0007FFUL;
	_topAddressForPartNumber[MB85RS64]		= 0x001FFFUL;
	_topAddressForPartNumber[MB85RS128A]	= 0x003FFFUL;
	_topAddressForPartNumber[MB85RS128B]	= 0x003FFFUL;
	_topAddressForPartNumber[MB85RS256A]	= 0x007FFFUL;
	_topAddressForPartNumber[MB85RS256B]	= 0x007FFFUL;
	_topAddressForPartNumber[MB85RS1MT]		= 0x01FFFFUL;
	_topAddressForPartNumber[MB85RS2MT]		= 0x03FFFFUL;
	
	_addressLengthForPartNumber[MB85RS16]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS64]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS128A]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS128B]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS256A]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS256B]	= ADDRESS16BIT;
	_addressLengthForPartNumber[MB85RS1MT]	= ADDRESS24BIT;
	_addressLengthForPartNumber[MB85RS2MT]	= ADDRESS24BIT;
	
	_densityCode[MB85RS16]					= 0x00; // RDID not supported
	_densityCode[MB85RS64]					= 0x00; // RDID not supported
	_densityCode[MB85RS128A]				= 0x00; // RDID not supported
	_densityCode[MB85RS128B]				= 0x04; // Density code in bits 4..0
	_densityCode[MB85RS256A]				= 0x00; // RDID not supported
	_densityCode[MB85RS256B]				= 0x05; // Density code in bits 4..0
	_densityCode[MB85RS1MT]					= 0x07; // Density code in bits 4..0
	_densityCode[MB85RS2MT]					= 0x08; // Density code in bits 4..0
	
	_baseAddress = 0x000000;
	_bottomAddress = _baseAddress + _maxBufferSize;
	_topAddress = _topAddressForPartNumber[_partNumber];
	_addressLength = _addressLengthForPartNumber[_partNumber];
	_numberOfBuffers = (_topAddress - _bottomAddress + 1) / _maxBufferSize;
	_nextFreeByte = _bottomAddress;
}


//
// PLATFORM SPECIFIC, LOW LEVEL METHODS
//

void Hackscribble_Ferro::_initialiseSPI(void)
{
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode (HS_SPI_DEFAULT_MODE);
	SPI.setClockDivider(HS_SPI_DEFAULT_CLOCK);
}
	
	
void Hackscribble_Ferro::_initialiseCS()
{	
		// Set the standard SS pin as an output to keep Arduino SPI happy
		pinMode (SS, OUTPUT);
		
	uint8_t timer = digitalPinToTimer(_chipSelect);
	_bit = digitalPinToBitMask(_chipSelect);
	uint8_t port = digitalPinToPort(_chipSelect);
	if (port == NOT_A_PIN) return;
	// If the pin that support PWM output, we need to turn it off
	// before doing a digital write.
	if (timer != NOT_ON_TIMER) turnOffPWM(timer);
	_out = portOutputRegister(port);
	
	
		pinMode (_chipSelect, OUTPUT);
		_deselect();

}

void Hackscribble_Ferro::_select()
{
	// digitalWrite(_chipSelect, LOW);
	uint8_t oldSREG = SREG;
	cli();
	*_out &= ~_bit;
	SREG = oldSREG;
}

void Hackscribble_Ferro::_deselect()
{
	// digitalWrite(_chipSelect, HIGH);
	uint8_t oldSREG = SREG;
	cli();
	*_out |= _bit;
	SREG = oldSREG;
}

uint8_t Hackscribble_Ferro::_readStatusRegister()
{
	uint8_t rxByte = 0;

	_select();
	SPI.transfer(_RDSR);
	rxByte = SPI.transfer(_dummy);
	_deselect();

	return rxByte;
}


void Hackscribble_Ferro::_writeStatusRegister(uint8_t value)
{
	_select();
	SPI.transfer(_WREN);
	_deselect();
	_select();
	SPI.transfer(_WRSR);
	SPI.transfer(value);
	_deselect();
}


void Hackscribble_Ferro::_readMemory(unsigned long address, uint8_t numberOfBytes, uint8_t *buffer)
{
	_select();
	SPI.transfer(_READ);
	if (_addressLength == ADDRESS24BIT)
	{
		SPI.transfer(address / 65536);
	}
	SPI.transfer(address / 256);
	SPI.transfer(address % 256);
	for (byte i = 0; i < numberOfBytes; i++)
	{
		buffer[i] = SPI.transfer(_dummy);
	}
	_deselect();
}


void Hackscribble_Ferro::_writeMemory(unsigned long address, uint8_t numberOfBytes, uint8_t *buffer)
{
	_select();
	SPI.transfer(_WREN);
	_deselect();

	_select();
	SPI.transfer(_WRITE);
	if (_addressLength == ADDRESS24BIT)
	{
		SPI.transfer(address / 65536);
	}
	SPI.transfer(address / 256);
	SPI.transfer(address % 256);
	for (byte i = 0; i < numberOfBytes; i++)
	{
		SPI.transfer(buffer[i]);
	}
	_deselect();
}


byte Hackscribble_Ferro::readProductID()
{
	// If the device supports RDID, returns density code (bits 4..0 of third byte of RDID response)
	// Otherwise, returns 0
	
	const byte densityMask = 0x1F; // Density code in bits 4..0
	byte densityCodeByte = 0x00;
	
	if (_densityCode[_partNumber] != 0x00)
	{
		_select();
		SPI.transfer(_RDID);
		SPI.transfer(_dummy); // Discard first two bytes
		SPI.transfer(_dummy); //
		densityCodeByte = SPI.transfer(_dummy) & densityMask;
		_deselect();
	}
	return densityCodeByte;
}


//
// PLATFORM INDEPENDENT, HIGH LEVEL METHODS
//

ferroResult Hackscribble_Ferro::begin()
{
	_initialiseCS();
	if (!_spiIsRunning)
	{
		_initialiseSPI();
		_spiIsRunning = true;
	}	
	return checkForFRAM();
}
	
	
ferroPartNumber Hackscribble_Ferro::getPartNumber()
{
	return _partNumber;
}


byte Hackscribble_Ferro::getMaxBufferSize()
{
	return _maxBufferSize;
}


unsigned long Hackscribble_Ferro::getBottomAddress()
{
	return _bottomAddress;
}


unsigned long Hackscribble_Ferro::getTopAddress()
{
	return _topAddress;
}


ferroResult Hackscribble_Ferro::checkForFRAM()
{
	// Tests that the unused status register bits can be read, inverted, written back and read again
	// If RDID is supported, tests that Product ID reported by device matches part number
		
	const byte srMask = 0x70; // Unused bits are bits 6..4
	byte registerValue = 0;
	byte newValue = 0;
		
	// Read current value
	registerValue = _readStatusRegister();
		
	// Invert current value
	newValue = registerValue ^ srMask;
		
	// Write new value
	_writeStatusRegister(newValue);
		
	// Read again
	registerValue = _readStatusRegister();
		
	if (readProductID() != _densityCode[_partNumber])
	{
		return ferroPartNumberMismatch;
	}
	else if (((registerValue & srMask) != (newValue & srMask)))
	{
		return ferroBadResponse;
	}
	else
	{
		return ferroOK;
	}
		
}
	
	
byte Hackscribble_Ferro::getControlBlockSize()
{
	return _maxBufferSize;
}


void Hackscribble_Ferro::writeControlBlock(uint8_t *buffer)
{
	_writeMemory(_baseAddress, _maxBufferSize, buffer);
}


void Hackscribble_Ferro::readControlBlock(uint8_t *buffer)
{
	_readMemory(_baseAddress, _maxBufferSize, buffer);
}


ferroResult Hackscribble_Ferro::read(unsigned long startAddress, byte numberOfBytes, byte *buffer)
{
	// Copies numberOfBytes bytes from FRAM (starting at startAddress) into buffer (starting at 0)
	// Returns result code
		
	// Validations:
	//		_bottomAddress <= startAddress <= _topAddress
	//		0 < numberOfBytes <= maxBuffer
	//		startAddress + numberOfBytes <= _topAddress
		
	if ((startAddress < _bottomAddress) || (startAddress > _topAddress))
	{
		return ferroBadStartAddress;
	}
	if ((numberOfBytes > _maxBufferSize) || (numberOfBytes == 0))
	{
		return ferroBadNumberOfBytes;
	}
	if ((startAddress + numberOfBytes - 1) > _topAddress)
	{
		return ferroBadFinishAddress;
	}

	_readMemory(startAddress, numberOfBytes, buffer);
			
	return ferroOK;
}


ferroResult Hackscribble_Ferro::write(unsigned long startAddress, byte numberOfBytes, byte *buffer)
{
	// Copies numberOfBytes bytes from buffer (starting at 0) into FRAM (starting at startAddress)
	// Returns result code
		
	// Validations:
	//		_bottomAddress <= startAddress <= _topAddress
	//		0 < numberOfBytes <= maxBuffer
	//		startAddress + numberOfBytes - 1 <= _topAddress
		
	if ((startAddress < _bottomAddress) || (startAddress > _topAddress))
	{
		return ferroBadStartAddress;
	}
	if ((numberOfBytes > _maxBufferSize) || (numberOfBytes == 0))
	{
		return ferroBadNumberOfBytes;
	}
	if ((startAddress + numberOfBytes - 1) > _topAddress)
	{
		return ferroBadFinishAddress;
	}

	_writeMemory(startAddress, numberOfBytes, buffer);
	
	return ferroOK;
}


unsigned long Hackscribble_Ferro::allocateMemory(unsigned long numberOfBytes, ferroResult& result)
{
	if ((_nextFreeByte + numberOfBytes) < _topAddress)
	{
		unsigned int base = _nextFreeByte;
		_nextFreeByte += numberOfBytes;
		result = ferroOK;
		return base;
	}
	else
	{
		result = ferroBadFinishAddress;
		return 0;
	}
}


ferroResult Hackscribble_Ferro::format()
{
	// Fills FRAM with 0 but does NOT overwrite control block
	// Returns result code from ferroWrite function, or ferroOK if format is successful

	byte buffer[_maxBufferSize];
		
	for (byte i = 0; i < _maxBufferSize; i++)
	{
		buffer[i] = 0;
	}
	
	ferroResult result = ferroOK;
	unsigned long i = _bottomAddress;
	while ((i < _topAddress) && (result == ferroOK))
	{
		result = write(i, _maxBufferSize, buffer);
		i += _maxBufferSize;
	}
	return result;
}


Hackscribble_FerroArray::Hackscribble_FerroArray(Hackscribble_Ferro& f, unsigned long numberOfElements, byte sizeOfElement, ferroResult &result): _f(f), _numberOfElements(numberOfElements), _sizeOfElement(sizeOfElement)
{
	// Creates array in FRAM
	// Calculates and allocates required memory
	// Returns result code
		
	// Validations:
	//		_sizeOfElement <= _bufferSize
		
	if (_sizeOfElement < _f.getMaxBufferSize())
	{
		_startAddress = _f.allocateMemory(_numberOfElements * _sizeOfElement, result);
	}
	else
	{
		result = ferroArrayElementTooBig;
		_startAddress = 0;
	}
		
}

	
void Hackscribble_FerroArray::readElement(unsigned long index, byte *buffer, ferroResult &result)
{
	// Reads element from array in FRAM
	// Returns result code
		
	// Validations:
	//		_startAddress > 0 (otherwise array has probably not been created)
	//		index < _numberOfElements
		
	if (_startAddress == 0)
	{
		result = ferroBadArrayStartAddress;
	}
	else if (index >= _numberOfElements)
	{
		result = ferroBadArrayIndex;
	}
	else
	{
		result = _f.read(_startAddress + (index * _sizeOfElement), _sizeOfElement, buffer);
	}
}

	
void Hackscribble_FerroArray::writeElement(unsigned long index, byte *buffer, ferroResult &result)
{
	// Writes element to array in FRAM
	// Returns result code
		
	// Validations:
	//		_startAddress > 0 (otherwise array has probably not been created)
	//		index < _numberOfElements
		
	if (_startAddress == 0)
	{
		result = ferroBadArrayStartAddress;
	}
	else if (index >= _numberOfElements)
	{
		result = ferroBadArrayIndex;
	}
	else
	{
		result = _f.write(_startAddress + (index * _sizeOfElement), _sizeOfElement, buffer);
	}
}

	
unsigned long Hackscribble_FerroArray::getStartAddress()
{
	return _startAddress;
}
