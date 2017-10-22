Hackscribble_Ferro Library
==========================

#### *Add fast, versatile, non-volatile ferroelectric memory (FRAM) to your Arduino. Simple hardware interface using SPI bus supports up to 256KB per FRAM chip.*

  
Created on 18 April 2014 by Ray Benitez  
Last modified on 19 June 2016 by Ray Benitez		
  
Please see **user guide.md** for more information on how to use the library.

This software is licensed by Ray Benitez under the MIT License.
	
git@hackscribble.com | http://www.hackscribble.com | http://www.twitter.com/hackscribble


 

### Change history

#### 1.51 (19 June 2016)

Fixed typo in example code fragment in user guide.

Added comment to user guide about support for MB85RS64V variant. 

#### 1.5 (3 December 2014)

Refactored SPI-related code in preparation for new ARM version of library.

Added example program for multiple FRAMs.

Corrected redundant variables in "arraytypes" example program.

Improved structure of example programs.

#### 1.4 (22 September 2014)

Changed memory addresses, array indices and array element sizes to support 24-bit addresses.

Added MB85RS1MT and MB85RS2MT to list of supported ICs.

Moved hardware initiation from constructor to begin().

Added validation of Product ID with part number (for devices supporting RDID opcode).

#### 1.3 (10 June 2014)

Improved read and write times.

Added option to "arraytypes" example sketch to allow testing of FRAM non-volatility.

Added "speedtest" and "arraytest" example sketches.

#### 1.2 (4 June 2014)
 
Fixed fault in begin() and checkForFRAM() affecting write access to high addresses in memory.

#### 1.1 (18 May 2014)
 
Fixed example folder structure.

#### 1.0 (18 April 2014)

Initial release.
