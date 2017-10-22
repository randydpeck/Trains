Hackscribble_Ferro Library User Guide
=====================================

#### *Add fast, versatile, non-volatile ferroelectric memory (FRAM) to your Arduino. Simple hardware interface using SPI bus supports up to 256KB per FRAM chip.*

<br>

Created on 18 April 2014 by Ray Benitez  
Last modified on 19 June 2016 by Ray Benitez  
Change history in **README.md**  

This software is licensed by Ray Benitez under the MIT License.  
  
git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble

<br>


1. About FRAM and the MB85RS range
----------------------------------

Ferroelectric RAM (FRAM) is a type of memory with the advantages of both ROM and RAM. You can read from and write to it at high speed like RAM.  It retains your data when the power is off, like ROM, without the need for a backup battery.  Unlike Flash memory or EEPROM, FRAM has a lifetime of 10 billion read and write operations.

FRAM is made by several manufacturers.  The Hackscribble_Ferro library is designed to work with the Fujitsu MB85RS range of FRAM and lets you add up to 32KB of fast, non-volatile storage to your Arduino.  You can read more about the MB85RS range and download datasheets here: http://www.fujitsu.com/global/services/microelectronics/product/memory/fram/standalone/#a4.

Since MB85RS is a bit of a mouthful, we are going to refer to it as just "the FRAM" from now on.


2. What the Hackscribble_Ferro library does
-------------------------------------------

The Hackscribble_Ferro library makes it simple to use the FRAM in your Arduino sketch, for temporary storage while the sketch is running, for long-term storage which keeps its contents even if the Arduino is powered off, or a combination of these.

The library has two classes which work together:

##### Hackscribble_Ferro

Hackscribble_Ferro controls the FRAM through the Arduino's SPI bus. You create an instance of Hackscribble_Ferro for each FRAM you connect to your Arduino: 

```
#include <Hackscribble_Ferro>
Hackscribble_Ferro myFerro;
ferroResult beginResult = myFerro.begin();
// It would be good to have some code here to check the result
```

Hackscribble_Ferro provides functions for writing blocks of data to the FRAM, reading them back and controlling the FRAM.  It assumes that your sketch will manage the details of how it uses the available FRAM memory.  

For many applications, however, you can skip these functions and instead use the second class in the library, Hackscribble_FerroArray, to do the managing for you.


##### Hackscribble_FerroArray

Hackscribble_FerroArray lets you create arrays of data which are stored in the FRAM. For each array, you choose the type of data (e.g. `byte`, `integer`, `float`, etc) and how many items are in the array (so long as you don't exceed the maximum size of the FRAM). 

Hackscribble_FerroArray takes care of how the arrays are stored in the FRAM, and you use simple functions to read and write your data.

For example, this is how you set up an array of 200 floating point numbers:

```
ferroResult myResult;
Hackscribble_FerroArray myFloatArray(myFerro, 200, sizeof(float), myResult); 
// It would be good to have some code here to check the result 
```

This is how you write the value of a variable called `myFloat` to the 12th position in this array:

```
myArray.WriteElement(12, (byte*)&myFloat, myResult);
```

This is how you read the value of the Nth element in the array, where `N` is a variable, and put it in `myFloat`:

```
myArray.ReadElement(N, (byte*)&myFloat, myResult);
```

We have tested this first release of Hackscribble_FerroArray with `boolean`, `char`, `byte`, `int`, `unsigned int`, `long`, `unsigned long`, `float`, character array (`char [ ]`) and user-defined structures (`struct`).    

> Don't worry about the `(byte*)&` stuff in front of the name of your variable.  You need to include it for Hackscribble_FerroArray to work but it doesn't stop you calling your variable what you want - you don't have to call it `myFloat`. And it's the same jargon whatever type your variable is.

When you create an array with Hackscribble_FerroArray, you are reserving memory in the FRAM to store your array.  Hackscribble_FerroArray does not erase the previous contents of the FRAM when it reserves this memory.  It only changes the contents of the FRAM memory when you write something new to it.  

Also, so long as you don't change how your arrays are defined, each time  you run your sketch, they will be stored in the same place in the FRAM. This means you can write data into your arrays, switch off your Arduino, run your sketch again some time later and read the same data out of the arrays.

Maybe you are not interested in this kind of "non-volatile" data storage.  For example, you might use the FRAM to hold data which your Arduino is capturing in real time from sensors and which you only process once enough has been collected.  In this case, you can initialise the arrays you create by writing zero (or another appropriate value) to each element before capturing new data, overwriting the previous contents.  

You can create some arrays to use as temporary storage and other arrays for non-volatile storage in the same FRAM.


3. Getting started
------------------

1.  Download and install the Hackscribble_Ferro library.

2.  Connect a FRAM to your Arduino.

3.  Try out the example sketches.

4.  Read the **Reference manual** section later in this guide to understand how to use the Hackscribble_Ferro library in your sketches.


4. How to install Hackscribble_Ferro
------------------------------------

Follow the standard Arduino instructions at http://arduino.cc/en/Guide/Libraries.


5. Connecting the FRAM to your Arduino
--------------------------------------

The MB85RS chips supported by the current version of this library are listed in the **Enumerated types** section later in this guide.  Datasheets for them are available on the Fujitsu website.  
  
With one exception, the supported chips operate at 3.3V.  You need to level convert their inputs and outputs to 5V before they connect to the Arduino.

![alt text](https://raw.githubusercontent.com/hackscribble/hackscribble-ferro-library/master/documentation/schematic.jpg "schematic")

You can build in level conversion to your circuit design or use an off-the-shelf converter breakout, such as this Adafruit 4-channel board: http://www.adafruit.com/products/757.

The supported MB85RS chips have 8 pins.  Connect them as follows:

Pin		| Connection								|
-------	|------------------------------------------ |
SI		| Arduino MOSI								|
SO		| Arduino MISO								|
SCK		| Arduino SCK								|
SS		| Arduino SS								|
WP		| Not used by the library - connect to Vcc	|
HOLD	| Not used by the library - connect to Vcc	|
Vcc		| +3.3V										|
GND		| 0V										|

You can connect two or more chips to your Arduino to increase the amount of storage. The chips can be any combination of the supported MB85RS types.  

Each SS pin needs to be level converted and connected to a separate pin on the Arduino. As explained in the **Reference manual** section, when you include the Hackscribble_Ferro library in your sketch, you tell it which Arduino pin to use for selecting each connected FRAM.    

For example, the standard SS pin on an Arduino Uno is pin 10.  You could choose to make pin 9 the chip select pin for your second FRAM. Your code would look like:

```
Hackscribble_Ferro ferro1(MB85RS256B, 10);
Hackscribble_Ferro ferro2(MB85RS256B, 9);
```


6. Example sketches
-------------------

The Hackscribble_Ferro library comes with four example sketches:

1. **test** checks that the FRAM has been connected correctly to your Arduino and then executes some of the key Hackscribble_Ferro and Hackscribble_FerroArray methods to check that the FRAM is working properly.

2. **arraytypes** creates arrays for a range of different data types, from `byte` to user-defined `struct`.  As well as checking that your FRAM is working properly, **arraytypes** is a good source of code snippets for you to include in your own sketches.

3. **arraytest** creates an array to fill the available memory and tests that all elements can be written and read correctly.

4. **multiple** is a simplified version of **arraytest** that tests up to three connected FRAMs.

5. **speedtest** measures the time taken to read from and write to the FRAM.


The sketches include comments and debug `Serial.println()` statements to explain what they are doing and what results you should expect.


7. Reference manual
-------------------

#### 7.1 Prerequisites

Include the following in your sketch to load the SPI and Hackscribble_Ferro libraries:

```
#include <SPI.h>
#include <Hackscribble_Ferro.h>
```


#### 7.2 Hackscribble_Ferro

##### Creating an instance of Hackscribble_Ferro

```
Hackscribble_Ferro(ferroPartNumber partNumber = MB85RS128A, byte chipSelect = SS);
```

The constructor takes two arguments, both of which have default values.  So there are three ways in which you can call the constructor.

The first way specifies non-default values for both `partNumber` and `chipSelect`.  For example ...

```
Hackscribble_Ferro myFerroInstance(MB85RS256B, 8);
```

... specifies the MB85RS256B model of FRAM and uses Arduino pin 8 as the chip select pin.

The second way specifies a non-default value for `partNumber`.  For example ...

```
Hackscribble_Ferro myFerroInstance(MB85RS256B);
```

... specifies the MB85RS256B model of FRAM but uses the default Arduino SS pin as the chip select pin.

The third way creates an instance with default values for both `partNumber` and `chipSelect`.

```
Hackscribble_Ferro myFerroInstance;
```

You can have more than one FRAM connected to your Arduino at the same time.  You must create each one with a different chip select pin.  For example ...

```
Hackscribble_Ferro ferro1(MB85RS256B, 8);
Hackscribble_Ferro ferro2(MB85RS256B, 9);
```

After creating the instance, you call:

```
ferroResult begin();
```

This initialises the SPI bus, so you don't have to in your sketch:

```
SPI.begin();
SPI.setBitOrder(MSBFIRST);
SPI.setDataMode (SPI_MODE0);
SPI.setClockDivider(SPI_CLOCK_DIV2);
```

It also checks that the Arduino can communicate with the FRAM (see `checkForFram()` below).

> If you are planning to use the Hackscribble_FerroArray methods, rather than managing the FRAM directly yourself, you can skip the rest of this section on Hackscribble_Ferro.  There are some methods which you may need to use if your application is complex, but most sketches will not need them.


##### Querying your FRAM

```
ferroResult checkForFRAM();
byte readProductID();
```

The `begin()` method calls `checkForFRAM()` to check that the Arduino can communicate with the FRAM.  The method is public so your sketch can use it too.  It works by reading, changing and re-reading unused bits in the FRAM status register. In this way, it can check two-way communication with the FRAM without interfering with your data.  

If the FRAM is one of the newer models that supports the RDID (read device ID) opcode, `checkForFRAM()` also calls `readProductID()`, which reads part of the identifying information from the FRAM and compares it with the part number specified in the program.   

`checkForFRAM()` returns either `ferroOK`, `ferroPartNumberMismatch` or `ferroBadResponse`.

```
ferroPartNumber getPartNumber();
```

Your sketch will have specified (either explicitly or implicitly) the part number of the FRAM you are using when creating the instance(s) of Hackscribble_Ferro.  However, you can use `getPartNumber()` to access the part number that has been configured.  

```
unsigned long getBottomAddress();
unsigned long getTopAddress();
byte getMaxBufferSize();
```

These three methods let your sketch find out the amount of memory available for your data. For example, if you are using an MB85RS128A, the following statements will usually display 64 and 16383 bytes.

```
Serial.println(myFerroInstance.getBottomAddress());
Serial.println(myFerroInstance.getTopAddress());
```

The maximum buffer size is the largest amount of data which can be transferred in a single operation to or from the FRAM using the `read()` and `write()` methods (see below).  The standard size is 64 bytes.


##### Reading and writing directly to FRAM

```
ferroResult read(unsigned long startAddress, byte numberOfBytes, byte *buffer);
ferroResult write(unsigned long startAddress, byte numberOfBytes, byte *buffer);
```

These methods transfer a block of data (group of adjacent bytes) between the Arduino SRAM and the FRAM.  `startAddress` specifies the address in FRAM of the first byte to be read or written.  `startAddress` must be between the values returned by `getBottomAddress()` and `getTopAddress()` inclusive.  The number of bytes to be read or written is passed in `numberOfBytes`. This cannot exceed the value returned by `getMaxBufferSize()`.  

Before using either `read()` or `write()`, you must allocate space in Arduino SRAM big enough to hold the largest block to be transferred to or from the FRAM.  For example:

```
Hackscribble_Ferro myFerro;
myFerro.begin();
byte ferroBuffer[myFerro.getMaxBufferSize()];
```

Now, the following code will write 32 bytes of data from the buffer to the FRAM, starting at FRAM address 2048:

```
// Code here to move your data into ferroBuffer
resultCode = myFerro.write(2048, 32, ferroBuffer);
```

And, this is how to read 48 bytes of data back from the FRAM, but starting at address 9999:

```
resultCode = myFerro.read(9999, 48, ferroBuffer);
// Code here to move your data out of ferroBuffer
```

> For clarity, we have not included any code in the snippets to check the result of each method call. You should make sure that your sketches have proper error handling.  Look at the example sketches for ideas on this.

When you `read()` and `write()`, the address in FRAM can be specified as a constant or by passing a variable to the method. Either way, the methods will not check if you have calculated the FRAM address correctly.  They will not prevent you reading data from an area of FRAM to which you have not previously written, nor prevent you accidentally overwriting data you meant to keep.  Your sketch must manage these aspects.

To help with this, each instance of Hackscribble_Ferro maintains, as a private variable, the address which it thinks is the next unused address in the available FRAM memory.  When the instance is created, this variable is set to the value returned by `getBottomAddress()`.  The variable increases when you call the `allocateMemory()` method.

```
unsigned long allocateMemory(unsigned long numberOfBytes, ferroResult& result);
```

This checks that there is room in the FRAM for you to reserve the next `numberOfBytes`.  If there is, it returns the current value of the private variable before increasing it by `numberOfBytes`.  Your calling routine can then make use of the memory starting at the returned address.  Provided it reads and writes between the returned address and (returned address + `numberOfBytes`), your sketch knows it will not bump into any other allocated blocks of memory.

The following code snippet and comments help to explain how the method works:
```
Serial.println(allocateMemory(1000, myResult);		// Assume this prints "64" as the start address for this new block
Serial.println(allocateMemory(2000, myResult);		// This will print "1064" (64 + 1000 for new block)
Serial.println(allocateMemory(999999, myResult);	// This will print "0" because I really should have tested the 
													// value of myResult, which will indicate that the method has 
													// failed because I have asked for too much memory
Serial.println(allocateMemory(42, myResult);		// This will print "3064" (1064 after first block + 2000 
													// for second block)
```

> To repeat, the `read()` and `write()` methods will not prevent you straying into memory you did not mean to.  The `allocateMemory()` method simply allocates a starting address and moves up the address of the next block to be allocated.  It does not guard the memory it allocates.  

If you need to wipe all the FRAM memory and fill it with zeroes, `format()` does so in a single instruction.

```
ferroResult format();
```


#### 7.3 Hackscribble_FerroArray

##### Creating an instance of Hackscribble_Ferro

```
Hackscribble_FerroArray(Hackscribble_Ferro& f, unsigned long numberOfElements, byte sizeOfElement, ferroResult &result);
```
The constructor takes four arguments.  The first is the reference to an instance of Hackscribble_Ferro which you must have created previously.  The last is a reference to a variable to take the result code after the method is run.

The second and third arguments specify the size of the array.  It will have `numberOfElements` elements, numbered (as in standard Arduino arrays) from 0 to (`numberOfElements` - 1).  `sizeOfElement` defines how many bytes each array element needs.  You could specify this as a literal numeric value, but it is clearer and safer if you use the `sizeof()` function with the type of variable you want to store in the array.  

For example, this is how you set up an array of 200 floating point numbers:

```
Hackscribble_FerroArray myFloatArray(myFerro, 200, sizeof(float), myResult); 
```

To create an array of user-defined structures (`struct`), you define the structure type first, and then use `sizeof()` when creating your array:

```
struct myStructType
{
	unsigned long frequency;
	unsigned int v1;
	unsigned int v2;
};
Hackscribble_FerroArray myStructArray(myFerro, 20, sizeof(myStructType), myResult);
```

So long as you do not exceed the available memory in the FRAM, you can create as many arrays as you like.


##### Reading from and writing to arrays

```
void readElement(unsigned long index, byte *buffer, ferroResult &result);
void writeElement(unsigned long index, byte *buffer, ferroResult &result);
```

To read or write to your arrays, you call the `readElement()` and `writeElement()` methods on the relevant array. You specify with `index` the element you want from 0 to (`numberOfElements` - 1), and you pass a pointer to the variable you want to read from or write to.  The variable must be of the same type as the elements in your array and it must be defined before you call these methods.

This is how you write the value of a variable called `myFloat` to the 12th position in this array:

```
myFloatArray.WriteElement(12, (byte*)&myFloat, myResult);
```

This is how you read the value of the Nth element in the array, where N is a variable, and put it in `myFloat`:

```
myFloatArray.ReadElement(N, (byte*)&myFloat, myResult);
```

> Putting `&` in front of `myFloat` gets the *address* of the `myFloat` variable in Arduino SRAM.  The compiler will remember that this is a pointer to a `float` variable.  However, we need `writeElement()` and `readElement()` to work with variables of all types.  Putting `(byte*)` in front of `&myFloat` casts the pointer into a byte pointer, no matter which actual variable type you are using.  

You must make sure that the variable you specify when calling these methods is of the same type as the array.  The following will compile correctly but will give unpredictable errors when you run your sketch:

```
Hackscribble_FerroArray myFloatArray(myFerro, 200, sizeof(float), myResult); 
int myInteger;
myFloatArray.ReadElement(N, (byte*)&myInteger, myResult);

```
We have included a method to find out the start address of an array in FRAM:

```
unsigned long getStartAddress();
```

This is useful for debugging but we do not recommend you use it in your sketches.  `readElement()` and `writeElement()` are all you need to access the data in your arrays.


#### 7.4 Control block

The Hackscribble_Ferro library reserves the first few bytes of FRAM memory as a control block.  You cannot read from or write to it (deliberately or accidentally) using the `read()`, `write()`, `readElement()` or `writeElement`methods.  You have to use the special methods in this section.

The reason we included a control block was to help with the practical challenges of releasing new versions of sketches that use non-volatile memory.  

For example, say you write Version 1 of a sketch which uses some of the FRAM for storing user settings or other data between sessions.  Imagine you update your sketch to Version 2, making changes to the design of the arrays you use to hold this data. 

If you upload Version 2 on an Arduino that has previously run Version 1, the sketch could read invalid data when it reads from the arrays in FRAM.

A better approach would be for your sketch to be able to check which version of the sketch was last run on the Arduino.  

- If it was Version 1, then your Version 2 sketch knows it must erase old data in the arrays before storing new user settings.
  
- If it was Version 2, then your sketch can safely read the previous user settings from the arrays.

The control block is a safe place for your sketch to store information such as version number.

```
byte getControlBlockSize();
```

The control block is 64 bytes long in this version of the library.  To check the size at run time, use `getControlBlockSize()`.

```
void readControlBlock(byte *buffer);
void writeControlBlock(byte *buffer);
```

To read from or write to the control block, you define a byte array in your sketch and pass it to `readControlBlock()` or `writeControlBlock()`.

```
// Get the control block size
byte myControlBufferSize = myFerro.getControlBlockSize();
// Create a buffer in memory  
byte ferroControlBuffer[myControlBufferSize];
// For demo purposes, fill the buffer with an arbitrary value
for (byte i = 0; i < myControlBufferSize; i++)
{
	ferroControlBuffer[i] = 123; 
}
// Write to the control block
myFerro.writeControlBlock(ferroControlBuffer);
```


#### 7.5 Enumerated types

This version of the library supports the following MB85RS variants: 

```
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
	numberOfPartNumbers  };
```
> The MB85RS64V device (5V version of the MB85RS64) is also supported. Use `MB85RS64` in the constructor.

Methods in Hackscribble_Ferro and Hackscribble_FerroArray return the following standard result codes.  Most of these can only happen if you are using the lower level methods in Hackscribble_Ferro to manage the FRAM yourself.  
```
enum ferroResult
{
	ferroOK = 0,
	ferroBadStartAddress,		// Tried to write to a starting address in FRAM outside the available memory range
	ferroBadNumberOfBytes,		// Tried to write more bytes than maxBufferSize, or zero bytes
	ferroBadFinishAddress,		// Tried to write data where the starting address is OK but the finish address is outside the available memory 
	ferroArrayElementTooBig,	// Tried to create an array where each element was bigger than maxBufferSize
	ferroBadArrayIndex,			// Tried to access a higher number element than you declared when creating the array
	ferroBadArrayStartAddress,	// Problem when reading or writing array element, probably due to earlier error in array creation
	ferroBadResponse,			// FRAM did not respond correctly (or at all) to checkForFram()
	ferroPartNumberMismatch,	// Product ID read from device did not match selected part number
	ferroUnknownError = 99		// Should not occur :-)
};
```
