/*

	speedtest
	=========
	
	Example sketch for Hackscribble_Ferro Library.
	
	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".
	
	
	Created on 8 May 2014
	By Ray Benitez
	Last modified on 18 September 2014
	By Ray Benitez
	Change history in "README.md"
		
	This software is licensed by Ray Benitez under the MIT License.

	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble


*/

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Enable or disable FRAM reads and writes
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define READ_WRITE_FRAM


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
	Serial.println(F("\n\n\n\n\nHACKSCRIBBLE_FERRO SPEED TEST"));
	Serial.println(F("==============================\n"));
	
#ifndef READ_WRITE_FRAM

	Serial.println(F("Neither reading nor writing FRAM - for calibration purposes"));
	
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
		unsigned int myBufferSize = myFerro.getMaxBufferSize(); 
		unsigned int myBottom = myFerro.getBottomAddress();
		unsigned int myTop = myFerro.getTopAddress();
		

		// FRAM SPEED TESTS
		
		uint32_t startTime, duration;
		const uint32_t outerLoopLimit = 50;
		const uint32_t arraySize = 2000;
		uint16_t testItem = 42;

		Hackscribble_FerroArray myArray(myFerro, arraySize, sizeof(testItem), myResult);
		
		
		// TEST 1
		// Write test data to integer array

		Serial.println(F("\nTest: repeatedly write to every array element"));
		Serial.println(F("Result ..."));

		// ADD ERROR HANDLING
		
		startTime = millis();
		
		for (uint16_t i = 0; i < outerLoopLimit; i++)
		{
			for (uint16_t j = 0; j < arraySize; j++)
			{
#ifdef READ_WRITE_FRAM
				myArray.writeElement(j, (uint8_t*)&testItem, myResult);
#endif				
			}	
		}
		
        duration = millis() - startTime;
        Serial.print("Number of write operations: ");
        uint32_t numberOfOperations = outerLoopLimit * arraySize;
        Serial.println(numberOfOperations);
        Serial.print("Bytes per write operation: ");
        Serial.println(sizeof(testItem));
        Serial.print("Duration: ");
        Serial.print(duration);
		Serial.println(" ms");
        Serial.print("Average time per operation: ");
        float averageTimeMicroSeconds = (1000.0 * duration) / numberOfOperations;
        Serial.print(averageTimeMicroSeconds, 1);
		Serial.println(" us");


		// TEST 1
		// Read test data from integer array

		Serial.println(F("\nTest: repeatedly read from every array element"));
		Serial.println(F("Result ..."));

		// ADD ERROR HANDLING
		
		startTime = millis();
		
		for (uint16_t i = 0; i < outerLoopLimit; i++)
		{
			for (uint16_t j = 0; j < arraySize; j++)
			{
#ifdef READ_WRITE_FRAM
				myArray.readElement(j, (uint8_t*)&testItem, myResult);
#endif				
			}	
		}
		
        duration = millis() - startTime;
        Serial.print("Number of read operations: ");
        Serial.println(numberOfOperations);
        Serial.print("Bytes per read operation: ");
        Serial.println(sizeof(testItem));
        Serial.print("Duration: ");
        Serial.print(duration);
		Serial.println(" ms");
        Serial.print("Average time per operation: ");
        averageTimeMicroSeconds = (1000.0 * duration) / numberOfOperations;
        Serial.print(averageTimeMicroSeconds, 1);
		Serial.println(" us");
       
	   	Serial.println(F("\n\n\nEND OF TESTS"));
	   	Serial.println(F("============\n"));         
	}

}


void loop() 
{
  
  
}
