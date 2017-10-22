/*

	multiple
	========

	Example sketch for Hackscribble_Ferro Library.

	For information on how to install and use the library,
	read "Hackscribble_Ferro user guide.md".


	Created on 31 October 2014
	By Ray Benitez
	Last modified on ---
	By ---
	Change history in "README.md"

	This software is licensed by Ray Benitez under the MIT License.

	git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble


*/


#define FORCE_ERRORS


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


const uint8_t numChipPositions = 3;

const boolean chipInstalled[numChipPositions] =
{
	true,		// U1, CS = D8
	true,		// U2, CS = D9
	true 		// U3, CS = D10
};

const ferroPartNumber chipType[numChipPositions] =
{
	MB85RS1MT,	// U1
	MB85RS256B,	// U2
	MB85RS256B 	// U3
};

const uint8_t chipCSPin[numChipPositions] =
{
	8, 		// U1
	9, 		// U2
	10		// U3
};

char testPattern[numChipPositions][4] =
{
	"123",		// U1
	"ray", 		// U2
	"@@@"		// U3
};

boolean chipOK[numChipPositions] = {false, false, false};

Hackscribble_Ferro* chip[numChipPositions];

unsigned long chipBottomAddress[numChipPositions];
unsigned long chipTopAddress[numChipPositions];


void setup()
{
	
	Serial.begin(115200);
	Serial.println(F("\n\n\n\n\nHACKSCRIBBLE_FERRO MULTIPLE TEST"));
	Serial.println(F("=============================\n"));
	
	
	// MULTIPLE FRAM TESTS
	// TEST 1

	for (uint8_t i = 0; i < numChipPositions; i++)
	{
		if (chipInstalled[i])
		{
			chip[i] = new Hackscribble_Ferro(chipType[i], chipCSPin[i]);
			Serial.print("Chip ");
			Serial.print(i + 1);
			Serial.print(" [");
			printPartNumber(chipType[i]);
			Serial.print("] ");
			if (chip[i]->begin() != ferroOK)
			{
				Serial.println("bad response");
			}
			else
			{
				Serial.println("response OK");
				chipOK[i] = true;
				chipBottomAddress[i] = chip[i]->getBottomAddress();
				chipTopAddress[i] = chip[i]->getTopAddress();
			}
		}
	}

	Serial.println("\r\nWrite started");

	for (unsigned long a = 0; a < 0x03FFFF; a+= sizeof(unsigned long))
	{
		for (uint8_t j = 0; j < numChipPositions; j++)
		{
			if (chipOK[j] && (a >= chipBottomAddress[j]) && (a <= chipTopAddress[j]))
			{
				chip[j]->write(a, 4, (uint8_t*)&testPattern[j]);

#ifdef FORCE_ERRORS
				if (a == 0x000420)
				{
					char errorPattern[] = "$$$";
					chip[j]->write(a, 4, (uint8_t*)errorPattern);
					Serial.print("Forcing error '");
					Serial.print(errorPattern);
					Serial.println("'");
				}
#endif

			}
		}
	}

	Serial.println("Write finished");

	char readString[4] = "uuu";

	Serial.println("\r\nRead started");

	for (unsigned long a = 0; a < 0x03FFFF; a += sizeof(unsigned long))
	{
		for (uint8_t j = 0; j < numChipPositions; j++)
		{
			if (chipOK[j] && (a >= chipBottomAddress[j]) && (a <= chipTopAddress[j]))
			{
				chip[j]->read(a, 4, (uint8_t*)&readString);
				if (memcmp(readString, testPattern[j], 4) != 0)
				{
					Serial.print("Error in chip ");
					Serial.print(j + 1);
					Serial.print(" at address 0x");
					Serial.print(a, HEX);
					Serial.print(": test pattern = '");
					Serial.print(testPattern[j]);
					Serial.print("', value read = '");
					Serial.print(readString);
					Serial.println("'");
				}
			}
		}
	}

	Serial.println("Read finished");
		
	Serial.println(F("\n\n\nEND OF TESTS"));
	Serial.println(F("============\n"));

}


void loop()
{
	
	
}
