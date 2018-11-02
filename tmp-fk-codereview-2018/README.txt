==============================================

mEngima Code review log README.txt

==============================================

Goals:

1. Learn code base 

2. additional documentation

3. possible code reduction by combining 6-9 main en/decode loop in fewer
callout routines
- might make code more difficult, but the goal is to free up some space

-----------------------------------------------

Starting by gathering code metrics using cloc and sloccount

cloc

      25 text files.
      classified 25 files
      24 unique files.                              
      11 files ignored.

github.com/AlDanial/cloc v 1.70  T=0.26 s (65.8 files/s, 24901.9 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
Arduino Sketch                   6            510           1078           3570
C/C++ Header                     5             27            103            443
C++                              1             51            159            294
Bourne Shell                     2             14            102             59
Markdown                         3              5              0             14
-------------------------------------------------------------------------------
SUM:                            17            607           1442           4380
-------------------------------------------------------------------------------

sloccount:


Have a non-directory at the top, so creating directory top_dir
Adding /home/frankk/mEnigma/source/./README.txt to top_dir
Adding /home/frankk/mEnigma/source/./cloc.txt to top_dir
Creating filelist for code-review-2018
Creating filelist for libraries
Creating filelist for meinEnigma
Adding /home/frankk/mEnigma/source/./sloccount.txt to top_dir
Categorizing files.
Finding a working MD5 command....
Found a working MD5 command.
Computing results.


SLOC	Directory	SLOC-by-Language (Sorted)
829     libraries       cpp=539,sh=290
341     meinEnigma      ansic=200,sh=141
0       code-review-2018 (none)
0       top_dir         (none)


Totals grouped by language (dominant language first):
cpp:            539 (46.07%)
sh:             431 (36.84%)
ansic:          200 (17.09%)




Total Physical Source Lines of Code (SLOC)                = 1,170
Development Effort Estimate, Person-Years (Person-Months) = 0.24 (2.83)
 (Basic COCOMO model, Person-Months = 2.4 * (KSLOC**1.05))
Schedule Estimate, Years (Months)                         = 0.31 (3.71)
 (Basic COCOMO model, Months = 2.5 * (person-months**0.38))
Estimated Average Number of Developers (Effort/Schedule)  = 0.76
Total Estimated Cost to Develop                           = $ 31,859
 (average salary = $56,286/year, overhead = 2.40).
SLOCCount, Copyright (C) 2001-2004 David A. Wheeler
SLOCCount is Open Source Software/Free Software, licensed under the GNU GPL.
SLOCCount comes with ABSOLUTELY NO WARRANTY, and you are welcome to
redistribute it under certain conditions as specified by the GNU GPL license;
see the documentation for details.
Please credit this data as "generated using David A. Wheeler's 'SLOCCount'."


-----------------------------------------------

Inorder to use

Fist minor finding..  ...what? forgot

IDE
---
- use of libraries in ide not documented by PS but OSS outside of scope
- needed to add to Altserial and Ht libraries to the sketch
COM default for IDE is 9600 needs to be 38400

Section code:

setup()
Serial.begin(38400);

Worked when changed to 9600

Licence Documentation?
----------------------

Code Findings (not good or bad just Frank notes)

Missing:

/* An Alternative Software Serial Library
 * http://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
 * Copyright (c) 2014 PJRC.COM, LLC, Paul Stoffregen, paul@pjrc.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 

Library Status - AltSoftSerial
------------------------------

Library version built with 1.2

Current version 1.4

https://github.com/PaulStoffregen/AltSoftSerial


Version
-------

Code use of 2 different Version strings... one is in serial ascii echo code the other is for LED print

~line 114
#define CODE_VERSION 107

used in 

printVersion()

DIFFERENTLY
~line 4420
case 'V': // Show version CODE_VERSION but making that dynamic requires a lot of code
	  displayString("V107",0);

Serial Input
------------
Only Uppercase BOGUS both work???
- FK check code for Upper/Lower trick

When using IDE session connectivity gets lost after power changes

Changed Baud from 38400

Message - CheckSum?
-------------------

- I Don't like message  "...seems OK..."
What is the use of check Sum?

Questions for peter
-------------------

Mine Powering on is doing morse code at start up (LOUD)
Is it true when serial mode the letters "EN I" are on LEDs? or Serial LED notication?
pressing V alernates between V and "EN I" when clock is off
add letting 0 to time? 24 hour time? add decimal on 4th char for AM or PM or 1st char
volume seeting????

Misc Notes:
-----------

BN: Unknown board
VID: 1A86
PID: 7523
SN: Upload any sketch to obtain it

due to fake board???

No peter said to google it....

seems yes...

http://acoptex.com/project/63/chinese-clone-of-arduino-nano-with-chip-ch340g-how-to-fix-it-at-acoptexcom/


Found Jeff Trantors led code:

http://jefftranter.blogspot.com/2017/05/programming-meinenigma-discrete-leds.html

created: 

Jeff-Trantor-LampAndKeyboardtest.ino


Fixed - Unable to flash:

Arduino: 1.8.7 (Windows Store 1.8.15.0) (Windows 10), Board: "Arduino Nano, ATmega328P"

Sketch uses 29332 bytes (95%) of program storage space. Maximum is 30720 bytes.
Global variables use 1146 bytes (55%) of dynamic memory, leaving 902 bytes for local variables. Maximum is 2048 bytes.
avrdude: stk500_getsync() attempt 1 of 10: not in sync: resp=0x1c
avrdude: stk500_getsync() attempt 2 of 10: not in sync: resp=0xe0
avrdude: stk500_getsync() attempt 3 of 10: not in sync: resp=0x1c
avrdude: stk500_getsync() attempt 4 of 10: not in sync: resp=0xe0
avrdude: stk500_getsync() attempt 5 of 10: not in sync: resp=0x1c
avrdude: stk500_getsync() attempt 6 of 10: not in sync: resp=0xe0
avrdude: stk500_getsync() attempt 7 of 10: not in sync: resp=0x1c
avrdude: stk500_getsync() attempt 8 of 10: not in sync: resp=0xfc
avrdude: stk500_getsync() attempt 9 of 10: not in sync: resp=0x1c
avrdude: stk500_getsync() attempt 10 of 10: not in sync: resp=0x00
An error occurred while uploading the sketch

This report would have more information with
"Show verbose output during compilation"
option enabled in File -> Preferences.


fixed:

 menu choices (Tools > Board > ATmega328P  -- and Tools > Board > ATmega328P (Old Bootloader)
 
 
FK PROPOSED ENHANCHEMENTS
-------------------------

- Change baud to 9600 to fit defaults of IDE


- Change Version to one value (if possible) put in EEPROM to save space?
- Put in checksum? save to eeprom. checksum isn't real?
- Change message from: "Checksum seems OK ... (1A2) keeping the values"
to
"Valid checksum XXX. Using existing values."

- Change "Found a soundboard" to "Found soundboard" saves space and is more accurate

- Echo found RTC in !SE

- Change MeinEngima to meinEnigma

- asciifont-pinout11.h to 
- asscifont-PSC05-11RWA.h

- asciifont-pinout12.h to 
- asscifont-PSC05-12RWA.h

update names in .ino file as well

Update comments to FixFont.sh
#
# FixFont.sh
#
# Read from stdin
# send to stdout
# flip some bits on it's way
#
# The intent is to fix the fontpart due to the different pinout of PSC05-11SRWA and PSC05-12SRWA
# If changing alnum type the pcb also need to change so it's not just in software it changes
# Look at the pinout of this two digits, note where the common is and then how all segments are changed around pinwise
# http://www.kingbrightusa.com/images/catalog/SPEC/PSC05-11SRWA.pdf
# http://www.kingbrightusa.com/images/catalog/SPEC/PSC05-12SRWA.pdf
#
# FK I think this is only used for the 2 .h files?
# asciifont-pinout11.h asciifont-pinout12.h

- add code to print valid rtc

existing:

#ifdef CLOCK
  uint16_t minute=i2c_read2(DS3231_ADDR,1);
  if (minute==0xFFFF){
    clock_active=missing; // no clock
    Serial.print(F("No RTC Found."));
  }else{
    Serial.print(F("Time is: "));
    printTime();
    Serial.println();
  }
  
 =========
 
 Need to drop CLOCK ifdef as all include them
 Need to drop SoundBoard ifdef as all include them
 
 Move baud rate setting outside of SoundBoard ifdef
 
 Should add define for BAUD
 
 " no soundboard found"
 to
 " No soundboard found"
 
 
 "No IC detected,assuming standalone"
 
to

"No IC detected,assuming standalone. BRB disabled."

   altSerial.begin(9600); was set earlier in code deleting this one...
   
"Preparing plugboard"

to

"Looking for plugboard"

for consistency

Serial.println(F("No IC detected,assuming standalone. BRB disabled."));

to
Serial.println(F("No IC detected, assuming standalone. BRB disabled."));

"All LEDs on"

to

"Testing all LEDs on"



MAYBE COMPRESS ASCII to 4 BITS? to save space

"BAD checksum, setting defaults"

to
"BAD checksum, loading defaults"


