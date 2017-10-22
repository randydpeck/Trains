/*

	arraytypes
	==========
	
	Example sketch for Hackscribble_Ferro Library.
	
	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".
	
	
	Created on 18 April 2014
	By Ray Benitez
	Last modified on 28 April 2014
	By Ray Benitez
	Change history in "README.md"
		
	This software is licensed by Ray Benitez under the MIT License.

	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble


*/

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable or disable write instructions - enables testing of non-volatility
//////////////////////////////////////////////////////////////////////////////////////////////////////

// To test non-volatility of FRAM ...
//		Compile and run this sketch
//		Comment out following #define statement, which creates identical arrays but prevents writing to them
//		Recompile and rerun this sketch
// You should still see expected results
#define WRITE_TO_FRAM


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


void setup() 
{
	
	Serial.begin(115200);
	Serial.println(F("\n\n\n\n\nHACKSCRIBBLE_FERRO ARRAY TYPES"));
	Serial.println(F("==============================\n"));
	
#ifndef WRITE_TO_FRAM
	
	Serial.println(F("Not writing to FRAM - just reading values previously written"));
	
#endif
	
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
	Serial.println(F("Expected result: FRAM response OK"));
	Serial.println(F("Result ..."));
	
	if (myFerro.begin() == ferroBadResponse)
	{
		Serial.println(F("FRAM response not OK"));
	}
	else
	{
		Serial.println(F("FRAM response OK"));
		
		ferroResult myResult = ferroUnknownError;


		// FRAM ARRAY TESTS
		
		const byte		testCharArrayLength = 20;
		boolean			testBoolean, newBoolean;
		char			testChar, newChar;
		byte			testByte, newByte;
		int				testInt, newInt;
		unsigned int	testUInt, newUInt;
		long			testLong, newLong;
		unsigned long	testULong, newULong;
		float			testFloat, newFloat;
		char			testCharA[testCharArrayLength], newCharA[testCharArrayLength];
		
		myResult = ferroUnknownError;

		Serial.print(F("\n\n\nFREE MEMORY: "));
		Serial.println(freeRam());
		

		// TEST 1
		// Write test data to boolean array

		Serial.println(F("\nTest: create array of boolean, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = true"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myBooleanArray(myFerro, 10, sizeof(boolean), myResult);
		testBoolean = true;
		newBoolean = false;
		
#ifdef WRITE_TO_FRAM
		myBooleanArray.writeElement(0, (uint8_t*)&testBoolean, myResult);
#endif

		myBooleanArray.readElement(0, (uint8_t*)&newBoolean, myResult);
		if (newBoolean == true)
		{
			Serial.println("true");
		}
		else
		{
			Serial.println("false");
		}

		// TEST 2
		// Write test data to char array

		Serial.println(F("\nTest: create array of char, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = Z"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myCharArray(myFerro, 10, sizeof(char), myResult);
		testChar = 'Z';
		newChar = 'A';
		
#ifdef WRITE_TO_FRAM
		myCharArray.writeElement(1, (uint8_t*)&testChar, myResult);
#endif

		myCharArray.readElement(1, (uint8_t*)&newChar, myResult);
		Serial.println(newChar);

		// TEST 3
		// Write test data to byte array

		Serial.println(F("\nTest: create array of byte, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = 250"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myByteArray(myFerro, 10, sizeof(byte), myResult);
		testByte = 250;
		newByte = 42;

#ifdef WRITE_TO_FRAM
		myByteArray.writeElement(2, (uint8_t*)&testByte, myResult);
#endif

		myByteArray.readElement(2, (uint8_t*)&newByte, myResult);
		Serial.println(newByte);

		// TEST 4
		// Write test data to int array

		Serial.println(F("\nTest: create array of int, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = -12345"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myIntArray(myFerro, 10, sizeof(int), myResult);
		testInt = -12345;
		newInt = 4321;
		
#ifdef WRITE_TO_FRAM
		myIntArray.writeElement(3, (uint8_t*)&testInt, myResult);
#endif

		myIntArray.readElement(3, (uint8_t*)&newInt, myResult);
		Serial.println(newInt);

		// TEST 5
		// Write test data to unsigned int array

		Serial.println(F("\nTest: create array of unsigned int, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = 54321"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myUIntArray(myFerro, 10, sizeof(unsigned int), myResult);
		testUInt = 54321;
		newUInt = 12345;


#ifdef WRITE_TO_FRAM
		myUIntArray.writeElement(4, (uint8_t*)&testUInt, myResult);
#endif

		myUIntArray.readElement(4, (uint8_t*)&newUInt, myResult);
		Serial.println(newUInt);
	
		// TEST 6
		// Write test data to long array

		Serial.println(F("\nTest: create array of long, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = -1234567"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myLongArray(myFerro, 10, sizeof(long), myResult);
		testLong = -1234567;
		newLong = 4321;


#ifdef WRITE_TO_FRAM
		myLongArray.writeElement(5, (uint8_t*)&testLong, myResult);
#endif

		myLongArray.readElement(5, (uint8_t*)&newLong, myResult);
		Serial.println(newLong);

		// TEST 7
		// Write test data to unsigned long array

		Serial.println(F("\nTest: create array of unsigned long, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = 3213213"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myULongArray(myFerro, 10, sizeof(unsigned long), myResult);
		testULong = 3213213;
		newULong = 12345;


#ifdef WRITE_TO_FRAM
		myULongArray.writeElement(6, (uint8_t*)&testULong, myResult);
#endif

		myULongArray.readElement(6, (uint8_t*)&newULong, myResult);
		Serial.println(newULong);

		// TEST 8
		// Write test data to float array

		Serial.println(F("\nTest: create array of float, write a value to it and read the value back"));
		Serial.println(F("Expected result: read value =  3.14159"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myFloatArray(myFerro, 10, sizeof(float), myResult);
		testFloat = 3.14159;
		newFloat = 12345;


#ifdef WRITE_TO_FRAM
		myFloatArray.writeElement(7, (uint8_t*)&testFloat, myResult);
#endif

		myFloatArray.readElement(7, (uint8_t*)&newFloat, myResult);
		Serial.println(newFloat, 5);
		
		// TEST 9
		// Write test data to character array (string) array

		Serial.println(F("\nTest: create array of char[], write a value to it and read the value back"));
		Serial.println(F("Expected result: read value = 1234567890123456789"));
		Serial.println(F("Result ..."));
		Hackscribble_FerroArray myCharAArray(myFerro, 10, sizeof(char[testCharArrayLength]), myResult);
		strcpy(testCharA, "1234567890123456789");
		strcpy(newCharA,  "AAAAAAAAAAAAAAAAAAA");


#ifdef WRITE_TO_FRAM
		myCharAArray.writeElement(8, (uint8_t*)&testCharA, myResult);
#endif

		myCharAArray.readElement(8, (uint8_t*)&newCharA, myResult);
		Serial.println(newCharA);
		
		Serial.print(F("\nFREE MEMORY: "));
		Serial.println(freeRam());
		
		Serial.println(F("\n\n\nEND OF TESTS"));
		Serial.println(F("============\n"));
	
	}

}


void loop() 
{
  
  
}
