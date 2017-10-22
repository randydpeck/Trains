/*
 
	arraytest
	=========
	
	Example sketch for Hackscribble_Ferro Library.
	
	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".
	
	
	Created on 29 May 2014
	By Ray Benitez
	Last modified on 22 September 2014
	By Ray Benitez
	Change history in "README.md"
		
	This software is licensed by Ray Benitez under the MIT License.
	
	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble

*/


// #define FORCE_ERRORS


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

void setup()
{
	
	Serial.begin(115200);
	Serial.println(F("\n\n\n\n\nHACKSCRIBBLE_FERRO ARRAY TEST"));
	Serial.println(F("=============================\n"));
	
	// Hackscribble_Ferro library uses standard Arduino SPI pin definitions:  MOSI, MISO, SCK.
	// Next statement creates an instance of Ferro using the standard Arduino SS pin and
	// default FRAM part number, MB85RS128A.
	Hackscribble_Ferro myFerro;

	// You specify a different FRAM part number like this ...
	// Hackscribble_Ferro myFerro(MB85RS256B);
	
	// If required, you can use a different chip select pin instead of SS, for example
	// when you have multiple devices on the SPI bus. You specify the pin like this ...
	// Hackscribble_Ferro myFerro(MB85RS128A, 9);
	
	
	// FRAM TESTS
	// TEST 1
	
	Serial.println(F("\nTest: check for correct response from FRAM"));
	Serial.println(F("Expected result: FRAM response OK; part number"));
	Serial.println(F("Result ..."));
	
	if (myFerro.begin() == ferroBadResponse)
	{
		Serial.println(F("FRAM response not OK"));
	}
	else
	{
		Serial.println(F("FRAM response OK"));
		printPartNumber(myFerro.getPartNumber());
		Serial.println();
		
		ferroResult myResult = ferroUnknownError;

		myFerro.format();


		// TEST 2
		
		unsigned long myBottom = myFerro.getBottomAddress();
		unsigned long myTop = myFerro.getTopAddress();
		
		unsigned long mySpace = myTop - myBottom;
		byte uintSize = sizeof(unsigned int);
		unsigned long myItems = mySpace / uintSize;
		Hackscribble_FerroArray myArray(myFerro, myItems, uintSize, myResult);
		unsigned int myData = 0;
		
		Serial.print("\n\n\nSpace available: ");
		Serial.println(mySpace);
		Serial.print("Size of data item: ");
		Serial.println(uintSize);
		Serial.print("Number of array elements: ");
		Serial.println(myItems);
		
		Serial.println(F("\nTest: write 0s into array and read to check"));
		Serial.println(F("Expected result: no error messages"));
		Serial.println(F("Result ..."));
		
		myData = 0;
		for (unsigned long i = 0; i < myItems; i++)
		{
			myArray.writeElement(i, (uint8_t*)&myData, myResult);
		}

#ifdef FORCE_ERRORS
			myData = 42;
			myArray.writeElement(myItems / 3, (uint8_t*)&myData, myResult);
#endif
		
		Serial.println("Start of checks");
		
		for (unsigned long i = 0; i < myItems; i++)
		{
			myArray.readElement(i, (uint8_t*)&myData, myResult);
			if (myData != 0)
			{
				Serial.print("Error: item ");
				Serial.print(i);
				Serial.print("  read value: ");
				Serial.println(myData);
			}
		}
		Serial.println("End of checks");
		
				
		// TEST 3
		
		Serial.println(F("\nTest: write incrementing values into array and read to check"));
		Serial.println(F("Expected result: no error messages"));
		Serial.println(F("Result ..."));
		
		for (unsigned long i = 0; i < myItems; i++)
		{
			myData = (int)((i * 2) % 65536UL);
			myArray.writeElement(i, (uint8_t*)&myData, myResult);
		}

#ifdef FORCE_ERRORS
		myData = 42;
		myArray.writeElement(2 * myItems / 3, (uint8_t*)&myData, myResult);
#endif
		
		Serial.println("Start of checks");
		for (unsigned long i = 0; i < myItems; i++)
		{
			myArray.readElement(i, (uint8_t*)&myData, myResult);
			if (myData != (int)((i * 2) % 65536UL))
			{
				Serial.print("Error: item ");
				Serial.print(i);
				Serial.print("  read value: ");
				Serial.println(myData);
			}
		}
		Serial.println("End of checks");
		
		

		Serial.println(F("\n\n\nEND OF TESTS"));
		Serial.println(F("============\n"));
		
	}

}


void loop()
{
	
	
}
