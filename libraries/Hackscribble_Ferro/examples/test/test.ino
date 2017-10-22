/*
 
	test
	====
	
	Example sketch for Hackscribble_Ferro Library.
	
	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".
	
	
	Created on 18 April 2014
	By Ray Benitez
	Last modified on 22 September 2014
	By Ray Benitez
	Change history in "README.md"
		
	This software is licensed by Ray Benitez under the MIT License.
	
	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble

*/


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Prerequisites for Hackscribble_Ferro
//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include <Hackscribble_Ferro.h>


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Free memory utility from www.arduino.cc
//////////////////////////////////////////////////////////////////////////////////////////////////////

int freeRam () {
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void printPartNumber(ferroPartNumber pn)
{
	switch(pn)
	{
		case MB85RS16:
		Serial.print("MB85RS16");
		break;
		case MB85RS64:
		Serial.print("MB85RS64");
		break;
		case MB85RS128A:
		Serial.print("MB85RS128A");
		break;
		case MB85RS128B:
		Serial.print("MB85RS128B");
		break;
		case MB85RS256A:
		Serial.print("MB85RS256A");
		break;
		case MB85RS256B:
		Serial.print("MB85RS256B");
		break;		
		case MB85RS1MT:
		Serial.print("MB85RS1MT");
		break;
		case MB85RS2MT:
		Serial.print("MB85RS2MT");
		break;
		default :
		Serial.print("unknown part number");
	}
}


#define PART_NUMBER MB85RS256B
#define PART_NAME	"MB85RS256B"


void setup() 
{
	
	Serial.begin(115200);
	Serial.println(F("\n\n\n\n\nHACKSCRIBBLE_FERRO TESTS"));
	Serial.println(F("========================\n"));
	
	// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
	// Next statement creates an instance of Ferro using the standard Arduino SS pin and
	// default FRAM part number, MB85RS128A.
	// Hackscribble_Ferro myFerro;

	// You specify a different FRAM part number like this ...
	Hackscribble_Ferro myFerro(PART_NUMBER);
	
	// If required, you can use a different chip select pin instead of SS, for example 
	// when you have multiple devices on the SPI bus. You specify the pin like this ...
	// Hackscribble_Ferro myFerro(MB85RS128A, 9);
	 
  
	// FRAM TESTS
	// TEST 1
	
	Serial.println(F("\nTest: check for correct response from FRAM"));
	Serial.print(F("Expected result: FRAM response OK; part number = "));
	Serial.println(PART_NAME);
	Serial.println(F("Result ..."));
	
	ferroResult res = myFerro.begin();
	
	if ( res == ferroBadResponse)
	{
		Serial.println(F("FRAM response not OK"));
	}
	else if (res == ferroPartNumberMismatch)
	{
		Serial.println(F("FRAM part number mismatch"));
		Serial.print(F("Density code reported by FRAM:  0x"));
        char b[10];
        sprintf(b,"%02X", myFerro.readProductID());
		Serial.println(b);
	}
	else
	{
		Serial.println(F("FRAM response OK"));
		printPartNumber(myFerro.getPartNumber());
		Serial.println();
		
		ferroResult myResult = ferroUnknownError;
		unsigned int myBufferSize = myFerro.getMaxBufferSize(); 
		unsigned long myBottom = myFerro.getBottomAddress();
		unsigned long myTop = myFerro.getTopAddress();


		// TEST 2
				
		byte ferroBuffer[myBufferSize];
	
		for (byte i = 0; i < myBufferSize; i++)
		{
			ferroBuffer[i] = 0;
		}
	
		Serial.println(F("\nTest: write zero-filled buffer to all available space in FRAM"));
		Serial.println(F("Expected result: incrementing start address for each write; all result codes = 0"));
		Serial.println(F("Result ..."));
		for (unsigned long i = myBottom; i < myTop; i += myBufferSize)
		{
			//Serial.print(i, HEX);
			//Serial.print(F(" "));

            char b[10];
            sprintf(b,"%06X ",i);
            Serial.print(b);
			Serial.print(myFerro.write(i, myBufferSize, ferroBuffer));
			Serial.print(F("  "));
			if ((i %1024) == 0)
			{
				Serial.println();
			}
		}
		Serial.println();

	
		// TEST 3
		
		const byte nRead = 20;

		Serial.println(F("\nTest: read subset of data back from FRAM"));
		Serial.println(F("Expected result: result code = 0; each value = 0"));
		Serial.println(F("Result ..."));
		// Read nRead bytes from an arbitrary address in FRAM to buffer
		Serial.println(myFerro.read(5150, nRead, ferroBuffer));
		
		for (byte i = 0; i < nRead; i++)
		{
			Serial.print(ferroBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();


		// TEST 4
		
		for (byte i = 0; i < myBufferSize; i++)
		{
			ferroBuffer[i] = (42 + i);
		}

		Serial.println(F("\nTest: fill buffer with test data"));
		Serial.println(F("Expected result: incrementing data starting at 42"));
		Serial.println(F("Result ..."));
	
		for (byte i = 0; i < myBufferSize; i++)
		{
			Serial.print(ferroBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();
		
		
		// TEST 5
		
		Serial.println(F("\nTest: write buffer to FRAM"));
		Serial.println(F("Expected result: result code = 0"));
		Serial.println(F("Result ..."));
		// Write 60 bytes from buffer to an arbitrary address in FRAM
		Serial.println(myFerro.write(5100, 60, ferroBuffer));
  
 		for (byte i = 0; i < myBufferSize; i++)
		{
			ferroBuffer[i] = 0;
		}
  
  
		// TEST 6
		
		Serial.println(F("\nTest: read subset of data back from FRAM"));
		Serial.println(F("Expected result: result code = 0; twenty incrementing values starting at 72"));
		Serial.println(F("Result ..."));
		Serial.println(myFerro.read(5130, nRead, ferroBuffer));
  
		for (byte i = 0; i < nRead; i++)
		{
			Serial.print(ferroBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();

	
		// FRAM CONTROL BLOCK TESTS
		// TEST 7

		byte myControlBufferSize = myFerro.getControlBlockSize();
		byte ferroControlBuffer[myControlBufferSize];
		
		for (byte i = 0; i < myControlBufferSize; i++)
		{
			ferroControlBuffer[i] = 123;
		}
		myFerro.writeControlBlock(ferroControlBuffer);

		for (byte i = 0; i < myControlBufferSize; i++)
		{
			ferroControlBuffer[i] = 255;
		}
		myFerro.readControlBlock(ferroControlBuffer);

		Serial.println(F("\nTest: read test data from control block"));
		Serial.println(F("Expected result: each value = 123"));
		Serial.println(F("Result ..."));
		for (byte i = 0; i < myControlBufferSize; i++)
		{
			Serial.print(ferroControlBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();


		// TEST 8
		
		for (byte i = 0; i < myControlBufferSize; i++)
		{
			ferroControlBuffer[i] = 43;
		}
		myFerro.writeControlBlock(ferroControlBuffer);

		for (byte i = 0; i < myControlBufferSize; i++)
		{
			ferroControlBuffer[i] = 255;
		}
				
		myFerro.readControlBlock(ferroControlBuffer);
		Serial.println(F("\nTest: read test data from control block"));
		Serial.println(F("Expected result: each value = 43"));
		Serial.println(F("Result ..."));
		for (byte i = 0; i < myControlBufferSize; i++)
		{
			Serial.print(ferroControlBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();


		// FRAM ARRAY TESTS
		// TEST 9

		struct testStruct
		{
			unsigned long frequency;
			unsigned int v1;
			unsigned int v2;
		};

		myResult = ferroUnknownError;
		unsigned long address1 = myFerro.getControlBlockSize();

		Serial.println(F("\nTest: create a FerroArray of struct"));
		Serial.print(F("Expected result: result code = 0; start address of array = "));
		Serial.println(address1);
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myArray(myFerro, 20, sizeof(testStruct), myResult);
		Serial.println(myResult);
		Serial.println(myArray.getStartAddress());
		unsigned long address2 = address1 + (20 * sizeof(testStruct));

		
		// TEST 10
			
		Serial.println(F("\nTest: create a FerroArray of float"));
		Serial.print(F("Expected result: result code = 0; start address of array = "));
		Serial.println(address2);
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray anotherArray(myFerro, 200, sizeof(float), myResult);
		Serial.println(myResult);
		Serial.println(anotherArray.getStartAddress());
		unsigned long address3 = address2 + (200 * sizeof(float));

	
		// TEST 11
		// Create this dummy array to check the size of the previous float array
		Serial.println(F("\nTest: create a FerroArray of int"));
		Serial.print(F("Expected result: start address of array = "));
		Serial.println(address3);
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray dummyArray(myFerro, 300, sizeof(int), myResult);
		Serial.println(dummyArray.getStartAddress());	

		Serial.print(F("\n\n\nFREE MEMORY: "));
		Serial.println(freeRam());
		
		// TEST 12
		// Write arbitrary test data to struct array
		testStruct myStruct = {14123456, 1234, 4320};
		testStruct newStruct = {0, 0.00, 0.00};
		
		Serial.println(F("\nTest: write struct with test data to array"));
		Serial.println(F("Expected result: 14123456, 1234, 4320; result code = 0"));
		Serial.println(F("Result ..."));
		Serial.print(myStruct.frequency);
		Serial.print(F("\t"));
		Serial.print(myStruct.v1);
		Serial.print(F("\t"));
		Serial.println(myStruct.v2);
		myArray.writeElement(12, (byte*)&myStruct, myResult);
		Serial.println(myResult);
	
	
		// TEST 13
		
		Serial.println(F("\nTest: read test data from array into a new struct"));
		Serial.println(F("Expected result: result code = 0; 14123456, 1234, 4320"));
		Serial.println(F("Result ..."));
		myArray.readElement(12, (byte*)&newStruct, myResult);
		Serial.println(myResult);
		Serial.print(newStruct.frequency);
		Serial.print(F("\t"));
		Serial.print(newStruct.v1);
		Serial.print(F("\t"));
		Serial.println(newStruct.v2);


		// TEST 14
		// Write arbitrary test data to float array
		float myFloat = 3.14;
		float newFloat = 0.0;
	
		Serial.println(F("\nTest: write float with test data to array"));
		Serial.println(F("Expected result: 3.14; result code = 0"));
		Serial.println(F("Result ..."));
		Serial.println(myFloat);
		anotherArray.writeElement(12, (uint8_t*)&myFloat, myResult);
		Serial.println(myResult);


		// TEST 15
		Serial.println(F("\nTest: read test data from array into a new struct"));
		Serial.println(F("Expected result: result code = 0; 3.14"));
		Serial.println(F("Result ..."));
		anotherArray.readElement(12, (uint8_t*)&newFloat, myResult);
		Serial.println(myResult);
		Serial.println(newFloat);
	

		// TEST 16
		// Write arbitrary test data to int array
		int16_t myInt = -123;
		int16_t newInt = 0;
		
		Serial.println(F("\nTest: write int with test data to array"));
		Serial.println(F("Expected result: -123; result code = 0"));
		Serial.println(F("Result ..."));
		Serial.println(myInt);
		dummyArray.writeElement(212, (uint8_t*)&myInt, myResult);
		Serial.println(myResult);


		// TEST 17
		Serial.println(F("\nTest: read test data from array into a new int"));
		Serial.println(F("Expected result: result code = 0; -123"));
		Serial.println(F("Result ..."));
		dummyArray.readElement(212, (uint8_t*)&newInt, myResult);
		Serial.println(myResult);
		Serial.println(newInt);


		// FRAM FORMAT TESTS
		// TEST 18

		myResult = ferroUnknownError;

		Serial.println(F("\nTest: format the FRAM and read subset of data"));
		Serial.println(F("Expected result: result codes = 0 and 0; each value = 0"));
		Serial.println(F("Result ..."));
		Serial.println(myFerro.format());
		Serial.println(myFerro.read(5130, nRead, ferroBuffer));
		
		for (byte i = 0; i < nRead; i++)
		{
			Serial.print(ferroBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();


		// TEST 19
		// Control block should not have been erased by format
		myFerro.readControlBlock(ferroControlBuffer);
		Serial.println(F("\nTest: read test data from control block"));
		Serial.println(F("Expected result: each value = 43"));
		Serial.println(F("Result ..."));
		for (byte i = 0; i < myControlBufferSize; i++)
		{
			Serial.print(ferroControlBuffer[i]);
			Serial.print(F("  "));
		}
		Serial.println();
	

		Serial.print(F("\n\n\nFREE MEMORY: "));
		Serial.println(freeRam());
		
		Serial.println(F("\n\n\nEND OF TESTS"));
		Serial.println(F("============\n"));
	
	}

}


void loop() 
{
  
  
}
