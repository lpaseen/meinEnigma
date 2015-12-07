/****************************************************************
   MyEnigma
   I'm writing this partly to learn programming. That means that
   this code is not optimized several things could be done in a 
   smarter way - I just don't know about them (yet).
   If it bugs you - fix it and tell me about it, that's the way I learn.

   Copyright: Peter Sjoberg <peters-src AT techwiz DOT ca>
   License: GPLv3
     This program is free software: you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 3 as 
     published by the Free Software Foundation.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Status: Pre Alpha, still adding functions to the code.

   History:
   v0.00 - test of library, keyboard and LEDs
   v0.01 - more parts now working, just missing wheel and enigma code.
   v0.02 - added wheel code


TODO: a lot but some "highligts"...
   Decide what order to count the walze/letters in
      right or left so the leftmost 4 wheel is wheel #4 
      or left to right so same wheel is #1 - or maybe #0
   Add switch code
   Add decimal point
   Add enigma crypto code
   fix so output is more like final version instead of debug mode


*/

#include "ht16k33.h"
#include "asciifont.h"
#include <EEPROM.h>
#include <util/crc16.h>
#include <Wire.h>

//Define the input pin for each of the 4 wheel encoders
//[wheel] [low, high]
//[wheel1low,wheel1high,wheel2low,wheel2hig...

#define WALZECNT 4
//if count is something else than 4 the pins this definitions also need to change
volatile uint8_t encoderPins[WALZECNT * 2] = {2, 3, 4, 5, 6, 7, 8, 9};
volatile uint8_t encoderState[WALZECNT] = {0xff, 0xff, 0xff, 0xff};
volatile boolean encoderMoved[WALZECNT] = {false, false, false, false};
//volatile uint8_t walze[WALZECNT] = {0, 0, 0, 0};

const char walzeContent[28] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";

typedef struct {
  char letter[26];         // 26 to allow external Uhr box connected, then A->B doesn't mean B->A.
} letters_t;

//enigma models
// http://www.cryptomuseum.com/crypto/enigma/timeline.htm
// Year  Army/Air   Navy UKW    Wheels
// 1933     I       -    UKW-A  I II III
// 1934     I       M1   UKW-A  
// 1937     I       M1   UKW-B  
// 1938     I       M2          IV V
// 1939     I       M2   UKW-B  Navy added  VI VII VIII
// 1940     I       M3   UKW-C 
// 1941     I       M4   
// 1944     I       M4   UKW-D  UHR for luftwaffe

typedef enum {M3,M4} enigmaModel_t;

//TODO: 
// Add something so we can have several presets in memory and then change between them

// EEPROM structure version - to be able to handle upgrades
#define VERSION 0

// rotor/wheel = walze
// Entry wheel = Eintrittswalze (EKW), static
// Reflector   = Umkehrwalze (UKW)
// wheels are counted from right to left so
//  rightmost wheel is 0
//  leftmost wheel is 3 and not available on M3

typedef struct {
  uint8_t fwVersion;         // firmware version
  uint8_t preset;            // Allow multiple saved settings
  enigmaModel_t model;
  uint8_t ukw;               // what reflector that is loaded
  uint8_t walze[WALZECNT];          // what wheel that currently is in the 3 or 4 positions
  char ringstellung[WALZECNT];      // Setting of the wheel ring.
  letters_t plugboard;
  unsigned long odometer;    // How many characters this unit has en/decrypted
  char currentWalze[WALZECNT];   // current position of the wheel, 0-sizeof(walzeContent)-2 not the letters!
  unsigned int nextlocation; // start for next section in eeprom
  boolean valid;             // whatever this section is valid or not
  unsigned int checksum;
} machineSettings_t;
// "nextLocation" and "valid" is used to cycle the eeprom writing location

machineSettings_t settings;

HT16K33 HT;
HT16K33::KEYDATA keys;

//  MCP23017 registers, all as seen from bank0
//
#define mcp_address 0x20 // I2C Address of MCP23017
#define IODIRA    0x00 // IO Direction Register Address of Port A
#define IODIRB    0x01 // IO Direction Register Address of Port B
#define IPOLA     0x02 // Input polarity port register 
#define IPOLB     0x03 // 
#define GPINTENA  0x04 // Interrupt on change
#define GPINTENB  0x05 // 
#define DEFVALA   0x06 // Default value register
#define DEFVALB   0x07 // 
#define INTCONA   0x08 // Interrupt on change control register
#define INTCONB   0x09 // 
#define IOCON     0x0A // Control register
#define IOCON     0x0B // 
#define GPPUA     0x0C // GPIO Pull-ip resistor register
#define GPPUB     0x0D // 
#define INTFA     0x0E // Interrupt flag register
#define INTFB     0x0F // 
#define INTCAPA   0x10 // Interrupt captred value for port register
#define INTCAPB   0x11 // 
#define GPIOA     0x12 // General purpose io register
#define GPIOB     0x13 // 
#define OLATA     0x14 // Output latch register
#define OLATB     0x15 // 



//Enigma lamp and Keyboard layout:
//   Q W E R T Z U I O
//    A S D F G H J K
//   P Y X C V B N M L

//The order the keys on the keyboard are coded
//Keyboard scancode table
//A-Z for keyboard, 1-9 for buttons
const char scancodes[] = "QWERTZUIOASDFGHJKPYXCVBNML123456789";

//A=65 so the [0] element contain the number of the LED that shows "A"
//after 'Z'-65 comes control LEDs [\]^_
//Usage: led['A'-65] or led[char-65]
const byte led[] = {16, 22, 4, 17, 19, 25, 20, 8, 14, 0, 18, 3, 5, 6, 7, 9, 10, 15, 24, 23, 2, 21, 1, 13, 12, 11, 26, 27, 28, 29, 30};

/****************************************************************/
// Write a single byte
uint8_t i2c_write(uint8_t unitaddr,uint8_t val){
  Wire.beginTransmission(unitaddr);
  Wire.write(val);
  return Wire.endTransmission();
}

/****************************************************************/
// Write two bytes
uint8_t i2c_write2(uint8_t unitaddr,uint8_t val1,uint8_t val2){
  Wire.beginTransmission(unitaddr);
  Wire.write(val1);
  Wire.write(val2);
  return Wire.endTransmission();
}

/****************************************************************/
// read a byte from specific address (send one byte(address to read) and read a byte)
uint8_t i2c_read(uint8_t unitaddr,uint8_t addr){
  i2c_write(unitaddr,addr);
  Wire.requestFrom(unitaddr,1);
  return Wire.read();    // read one byte
}

/****************************************************************/
// read 2 bytes starting at a specific address
// read is done in two sessions - as required by mcp23017 (page 5 in datasheet)
uint16_t i2c_read2(uint8_t unitaddr,uint8_t addr){
  uint16_t val;
  i2c_write(unitaddr,addr);
  Wire.requestFrom(unitaddr, 1);
  val=Wire.read();
  Wire.requestFrom(unitaddr, 1);
  return Wire.read()<<8|val;
}

/****************/
//calculate a checksum of a block
// basic function taken from https://www.arduino.cc/en/Tutorial/EEPROMCrc
unsigned int getCsum(void *block, uint8_t size) {
  unsigned char * data;
  unsigned int i, csum;
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  csum = 0;
  data = (unsigned char *) block;

  for (int index = 0 ; index < size  ; ++index) {
    crc = crc_table[(crc ^ data[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (data[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  
#ifdef DEBUG
  Serial.print(" getCsum: ");
  Serial.print(crc,HEX);
  Serial.print(F(" "));
  Serial.print(size,DEC);
  Serial.println();
#endif  
  
  return (int) crc;
} // getCsum

/****************************************************************/
// save the settings to eeprom
// use some trix of moving the save location across the eeprom to not 
// always write in one spot
void saveSettings() {
  machineSettings_t eesettings;
  uint8_t i;
  uint16_t addr;
  unsigned char *ptr = (unsigned char *)&settings;
  unsigned char *eeptr = (unsigned char *)&eesettings;

  eeprom_read_block((void*)&eesettings, (void*)settings.nextlocation, sizeof(eesettings));
  addr=settings.nextlocation; // save old location
  
  //suppose to write at a different location now and then but start with just update the current location
  //procedure:
  //read current
  // if valid
  //    - increase .nextlocation one sizeof
  //    - if to close to EEPROM.length() start over at 0
  //    - update TOBE location
  //    - set valid=false
  //    - update csum
  //    - update old location
  // else (current is invalid)
  //    - compare and save anything that changed
  //

  
  settings.nextlocation=0;
  settings.valid=true;
  settings.checksum = getCsum((void*) ptr, sizeof(settings) - 2);

  Serial.print(F("saveSettings at "));
  Serial.println(addr);
  eeprom_update_block((const void*)&settings, (void*)addr, sizeof(settings));
} // saveSettings

/****************************************************************/
// read the settings from eeprom
// we use some trix of moving the save location across the eeprom to not 
// always write in one spot and need to follow it if needed
uint8_t readSettings() {
  static uint16_t addr=0;
  unsigned int ccsum;
  unsigned char *ptr = (unsigned char *)&settings;

#ifdef DEBUG
  Serial.print("readSetting: ccsum=");
#endif
  //First read from first location
  eeprom_read_block((void*)&settings, (void*)addr, sizeof(settings));

  ccsum = getCsum((void*) ptr, sizeof(settings) - 2);
#ifdef DEBUG
  Serial.println(ccsum,HEX);
#endif
  while (ccsum == settings.checksum) { // if valid csum, check if block is valid
    if (settings.valid){
      settings.nextlocation=addr;// this is were the valid block is right now
      return 0;// we got valid data in the global "settings" structure (I hope)
    } else {
      addr=settings.nextlocation;
      eeprom_read_block((void*)&settings, (void*)addr, sizeof(settings));
      ccsum = getCsum((void*) ptr, sizeof(settings) - 2);
    }
  }
  settings.valid=false;
  return 1; // csum invalid so no valid data
} // readSettings

//Some code from http://playground.arduino.cc/Main/PinChangeInterrupt
//This is tested on an arduino uno and arduino nano, other models may not work
void pciSetup(uint8_t pin)
{
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
  PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
  PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

//BUG: not much to debounce code!
// and that seems to be needed, how to do that with interrupts?

void updateEncoderState() {
  uint8_t i, state, state2;

  //read state of all 4 encoders
  for (i = 0; i < sizeof(encoderPins); i += 2) {
    state = 0; // clear from prev round
    state = digitalRead(encoderPins[i + 1]) << 1 | digitalRead(encoderPins[i]);
    delay(10); //debounce
    state2 = digitalRead(encoderPins[i + 1]) << 1 | digitalRead(encoderPins[i]);
    if (state == state2) {
      if ((encoderState[i / 2] & 3) != state) { // same as before ?
        encoderState[i / 2] = encoderState[i / 2] << 2 | state;
        encoderMoved[i / 2] = true;
      } // if state changed
    } // if no bounce
  } // for
} //updateEncoderState

ISR (PCINT2_vect) { // handle pin change interrupt for D0 to D7 here
  updateEncoderState();
} // ISR D0-D7

ISR (PCINT0_vect) { // handle pin change interrupt for D8 to D13 here
  updateEncoderState();
} // ISR D8-D13

/****************************************************************/
void setup() {
  uint8_t i, j;
  unsigned long ccsum;
  unsigned char *ptr = (unsigned char *)&settings;

  Serial.begin(115200);
  Serial.println("My enigma v0.02");
  Serial.println();

  // load prev settings
  // from http://playground.arduino.cc/Code/EEPROMWriteAnything
  //  eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));

  //  if (ccsum != settings.checksum) {
  if (readSettings()!=0){
    Serial.println("BAD checksum, setting defaults");
    settings.fwVersion = VERSION;
    settings.preset = 0; // we can have several saved settings
    settings.model = M3;
    settings.ukw = 1;
    settings.walze[0] = 3; // wheel type,
    settings.walze[1] = 2;
    settings.walze[2] = 1;
    settings.walze[3] = 0;
    settings.ringstellung[0] = 0;
    settings.ringstellung[1] = 0;
    settings.ringstellung[2] = 0;
    settings.ringstellung[3] = 0;
    for (i = 0; i < sizeof(settings.plugboard); i++) {
      settings.plugboard.letter[i]=i;
    }
    settings.currentWalze[0] = 0; // 0 is space or blank
    settings.currentWalze[1] = 1; // 1 = "A"
    settings.currentWalze[2] = 1;
    settings.currentWalze[3] = 1;
    settings.nextlocation=0;
    settings.odometer = 0;
    //    eeprom_update_block((const void*)&settings, (void*)0, sizeof(settings));
    saveSettings();
  } else {
    Serial.print(F("Checksum seems ok, ("));
    Serial.print(settings.checksum,HEX);
    Serial.println(F(") keeping the values"));
  }

  Serial.print(F("fwVersion: "));
  Serial.println(settings.fwVersion, HEX);
  Serial.print(F("model: "));
  switch (settings.model) {
    case M3:
      Serial.println(F("M3"));
      break;
      ;;
    case M4:
      Serial.println(F("M4"));
      break;
      ;;
    default:
      Serial.print(F("Unknown: "));
      Serial.println(settings.model,DEC);
  }

  Serial.print(F("reflector: "));
  Serial.println(settings.ukw, DEC);
  Serial.print(F("Wheels: "));
  Serial.print(settings.walze[0], DEC);
  Serial.print(F(" "));
  Serial.print(settings.walze[1], DEC);
  Serial.print(F(" "));
  Serial.print(settings.walze[2], DEC);
  Serial.print(F(" "));
  Serial.println(settings.walze[3], DEC);
  Serial.print(F("ringstellung: "));
  Serial.print(walzeContent[settings.ringstellung[0]]);
  Serial.print(F(" "));
  Serial.print(walzeContent[settings.ringstellung[1]]);
  Serial.print(F(" "));
  Serial.print(walzeContent[settings.ringstellung[2]]);
  Serial.print(F(" "));
  Serial.println(walzeContent[settings.ringstellung[3]]);
  Serial.println(F("Plugboard (Steckerbrett): "));
  for (i = 0; i < sizeof(settings.plugboard); i++) {
    if (settings.plugboard.letter[i] != i) {
      Serial.print(F(" "));
      Serial.print(i,DEC);
      Serial.print(F("->"));
      Serial.println(settings.plugboard.letter[i],DEC);
    }
  }
  Serial.print(F("currentWalze: "));
  Serial.print(walzeContent[settings.currentWalze[0]]);
  Serial.print(F(" "));
  Serial.print(walzeContent[settings.currentWalze[1]]);
  Serial.print(F(" "));
  Serial.print(walzeContent[settings.currentWalze[2]]);
  Serial.print(F(" "));
  Serial.println(walzeContent[settings.currentWalze[3]]);
  Serial.print(F("Odo meter: "));
  Serial.println(settings.odometer, DEC);

  Wire.begin(); // enable the wire lib
  // Setup the port multipler
  i2c_write2(mcp_address,IOCON,0b00011110);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address,IODIRA,0xff); // Set all ports to inputs
  i2c_write2(mcp_address,IODIRB,0xff); // Set all ports to inputs
  i2c_write2(mcp_address,GPPUA,0); // disable pullup (for now,to save power)
  i2c_write2(mcp_address,GPPUB,0); //

  //The other chip
  i2c_write2(mcp_address+1,IOCON,0b00011110);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address+1,IODIRA,0xff); // Set all ports to inputs
  i2c_write2(mcp_address+1,IODIRB,0xff); // Set all ports to inputs
  i2c_write2(mcp_address+1,GPPUA,0); // disable pullup (for now,to save power)
  i2c_write2(mcp_address+1,GPPUB,0); //

  HT.begin(0x00);

  // Test the screen
  Serial.println(F("All LEDs on"));
  for (i = 0; i <= 128; i++) {
    HT.setLed(i);
    //    HT.sendLed();
  }
  HT.sendLed();
  delay(500);

  Serial.println(F("All LEDs OFF"));
  for (i = 0; i <= 128; i++) {
    HT.clearLed(i);
    //HT.sendLed();
    //    delay(100);
  }
  HT.sendLed();
  delay(500);

  // Clear decimal point
  for (i = 10; i <= 13; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, HIGH);
  }

  //Setup encoder wheel interrupt
  for (i = 0; i < sizeof(encoderPins); i++) {
    pinMode(encoderPins[i], INPUT_PULLUP);
    pciSetup(encoderPins[i]);
  }

} // setup

/****************************************************************/
uint8_t checkKB() {
  boolean ready;
  uint8_t i,keyflag;
  int8_t key;
  HT16K33::KEYDATA oldkeys;
  

  //PSDEBUG
#ifdef DEBUG
  //    ready=HT.keysPressed();
    //    if (ready != 0) {
      HT.readKeyRaw(oldkeys,false); // false to go from cache - what was read above
      key=HT.readKey(); // this will update cache but also clear the chip mem, "true" to ignore other keys
      HT.readKeyRaw(keys,false); // false to go from cache - what was read above
    if (key != 0) {
      Serial.print(F(" "));
      for (i = 0; i < 3; i++) {
	if (oldkeys[i] < 0x1000) {
	  Serial.print(F("0"));
	}
	if (oldkeys[i] < 0x100)  {
	  Serial.print(F("0"));
	}
	if (oldkeys[i] < 0x10)   {
	  Serial.print(F("0"));
	}
	Serial.print(oldkeys[i], HEX);
	Serial.print(F(" "));
      }
      Serial.println();

      Serial.print(F(" "));
      for (i = 0; i < 3; i++) {
	if (keys[i] < 0x1000) {
	  Serial.print(F("0"));
	}
	if (keys[i] < 0x100)  {
	  Serial.print(F("0"));
	}
	if (keys[i] < 0x10)   {
	  Serial.print(F("0"));
	}
	Serial.print(keys[i], HEX);
	Serial.print(F(" "));
      }
      Serial.println();
      Serial.print(F(", keyReady is: "));
      Serial.print(ready);
      Serial.print(F("  readkey is: "));
      Serial.print(key);
      Serial.println();
      return key;
#else
      //  if (HT.keyReady() != 0) {
    return HT.readKey();
    //  }
#endif
} // checkKB

/****************************************************************/
// Check the wheels, if they moved and something was updated
// Return true if any wheel changed
//
boolean checkWalzes() {
  uint8_t i;
  boolean changed;

  changed=false;
  for (i = 0; i < WALZECNT; i++) {
    if (encoderMoved[i]) {
      encoderMoved[i] = false; // ack the change
      // states when moving in direction A: 11 01 00 10
      // states when moving in direction B: 11 10 00 01
      // [prev][curr] state, that means it was moved
      // 1101 0100 0010 1011 dirA
      // 1110 1000 0001 0111 dirB
      switch (encoderState[i] & 0xf) {
        case B1101: // one quarter, starting to move it
        case B0100: // two quarter moved
        case B0010: // three quarter moved
	  break; // ignore the first 3 clicks
        case B1011: // next click landed
	  changed = true; // moved up
	  if (settings.currentWalze[i] == sizeof(walzeContent)-2) {  // index start at 0 and is terminated with \0
            settings.currentWalze[i] = 0;
          } else {
	    settings.currentWalze[i]++;
          }
          break;
        case B1110: // one quarter, start moving it
        case B1000: // two quarter moved
        case B0001: // three quarter moved
	  break; // ignore the first 3 clicks
        case B0111: // next click landed
          changed = true; // moved down
          if (settings.currentWalze[i] == 0) {
	    settings.currentWalze[i] = sizeof(walzeContent)-2;
          } else {
            settings.currentWalze[i]--;
          }
          break;
#ifdef DEBUG
      default: // unknown movement, possible started a move and fell back
          Serial.print(F("unknown: "));
#endif
      } // switch
    } // if moved
  } // for i in walzecnt
  return changed;
} // checkWalzes

/****************************************************************/
// display something on one of the "wheels"
// TODO: handle attributes like decimalpoint
//  rightmost wheel is 4
//  leftmost is 1 and not avaliable on model M3
//
void writeLetter(char letter, uint8_t walze) {
  uint8_t led;
  int8_t i;
  uint16_t val;

#ifdef DEBUG
  Serial.print(F(" Write wheel "));
  Serial.print(walze,DEC);
  Serial.print(F(" val "));
  Serial.println(letter,DEC);
#endif

  led = (walze - 1) * 16;
  val = fontTable[letter - 32]; // A= 0b1111001111000000 - 0xF3C0
  for (i = 15; i >= 0; i--) {
    if (val & (1 << i)) {
      HT.setLed(led + i);
    } else {
      HT.clearLed(led + i);
    } // if val
  } // for
  HT.sendLed();
} // writeLetter

/****************************************************************/
// scroll out message at the speed of "sleep"
// 
void writeString(char msg[], uint16_t sleep) {
  uint8_t i;
  for (i = 0; i < strlen(msg); i++) {
    writeLetter(msg[i], 4);
    if (i > 0) {
      writeLetter(msg[i - 1], 3);
    }
    if (i > 1) {
      writeLetter(msg[i - 2], 2);
    }
    if (i > 2) {
      writeLetter(msg[i - 3], 1);
    }
    delay(sleep);
  }
} // writeString

/****************************************************************/
// check the Steckerbrett
// update the plugboard array with what plugs that are connected

uint8_t checkPlugboard() {
  uint8_t plug,bit,mcp,port,i,plug2;
  uint16_t val,val1,val2;
  letters_t newplugboard;

  //TODO: A way to make the plugboard optional
  //      probably read the mcp and if it doesn't 
  //      look right declare plugboard not connected.

  // make all io ports inputs-pullup
  i2c_write2(mcp_address,GPPUA,0xff); // enable 100k pullup
  i2c_write2(mcp_address,GPPUB,0xff); //
  i2c_write2(mcp_address+1,GPPUA,0xff); // enable 100k pullup
  i2c_write2(mcp_address+1,GPPUB,0xff); //

  for (i = 0; i < sizeof(newplugboard); i++) {
    newplugboard.letter[i]=i;
  }

  //yes, it's inefficient code but it's a start
  for (plug=0;plug<sizeof(settings.plugboard);plug++){
    if (plug>=24){
      bit=plug-24;
      mcp=mcp_address+1;
      port=1;
    } else if (plug>=16){
      bit=plug-16;
      mcp=mcp_address+1;
      port=0;
    } else if (plug>=8){
      bit=plug-8;
      mcp=mcp_address;
      port=1;
    } else {
      bit=plug;
      mcp=mcp_address;
      port=0;
    }

    //make port "plug" output
    i2c_write2(mcp,IODIRA+port,0xff ^ (1<<bit));
    
    //set  port "plug" low
    i2c_write2(mcp,GPIOA+port,0xff ^ (1<<bit));

    val1=i2c_read2(mcp_address,GPIOA);
    val2=i2c_read2(mcp_address+1,GPIOA);
    //Quick check, need to optimize this code a lot
    for (i=0;i<16;i++){
      if ((val1 & (1<<i))==0){
	if (mcp==mcp_address){
	  if (port==0 && i<8){
	    if (bit==i){
	      //	      Serial.print(F("*"));
	      continue;
	    }
	  }else if (port==1 && i>=8){
	    if (bit==(i-8)){
	      //	      Serial.print(F("*"));
	      continue;
	    }
	  }
	}
	newplugboard.letter[plug]=i;
      }
    }

    for (i=0;i<16;i++){
      if ((val2 & (1<<i))==0){
	if (mcp==mcp_address+1){
	  if (port==0 && i<8){
	    if (bit==i){
	      //	      Serial.print(F("*"));
	      continue;
	    }
	  }else if (port==1 && i>=8){
	    if (bit==(i-8)){
	      //	      Serial.print(F("*"));
	      continue;
	    }
	  }
	}
	newplugboard.letter[plug]=i+16;
      }
    }

    //make port input again
    i2c_write2(mcp,GPIOA+port,0xff);
    i2c_write2(mcp,IODIRA+port,0xff);
    //    i2c_write2(mcp,GPPUA+port,0xff);
  }
  // disable all pullups to save power
  i2c_write2(mcp_address,GPPUA,0);
  i2c_write2(mcp_address,GPPUB,0);
  i2c_write2(mcp_address+1,GPPUA,0);
  i2c_write2(mcp_address+1,GPPUB,0);

  if (memcmp(newplugboard.letter, settings.plugboard.letter, sizeof(settings.plugboard)) != 0){
    //    memcpy(settings.plugboard, newplugboard,sizeof(settings.plugboard));
    settings.plugboard=newplugboard;
    saveSettings();
//PSDEBUG
#ifndef DEBUG
    Serial.println(F("New Plugboard (Steckerbrett) setting: "));
    for (i = 0; i < sizeof(settings.plugboard); i++) {
      if (settings.plugboard.letter[i] != i) {
	Serial.print(F(" "));
	Serial.print(i,DEC);
	Serial.print(F("->"));
	Serial.println(settings.plugboard.letter[i],DEC);
      }
    }
    Serial.println();
    delay(1000);
#endif
  }
} // checkPlugboard

/****************************************************************/
// update the wheels
 void updateWheels(){
   writeLetter(walzeContent[settings.currentWalze[0]],4);
   writeLetter(walzeContent[settings.currentWalze[1]],3);
   writeLetter(walzeContent[settings.currentWalze[2]],2);
   writeLetter(walzeContent[settings.currentWalze[3]],1);
 }  //


/****************************************************************/
/****************************************************************/
void loop() {
  static uint8_t led = 0;
  static uint8_t ledOn=0;
  int8_t i,key;
  uint16_t val,cnt;
  char j;
  char hello[] = "ENIGMA";

  pinMode(11, OUTPUT);
  digitalWrite(11, HIGH);

//  writeString("HELLO WORLD    ", 400);

  writeString(hello, 400);
  delay(500);
  writeString("V002",0);
  digitalWrite(11,LOW);
  delay(1000);
  digitalWrite(11,HIGH);
  writeString("    ",0);

  i=HIGH; // PSDEBUG: for heartbeat

  Serial.println();
  Serial.println("waiting");

  /*
   * wait for key pressed
   * once pressed, light up corresponding LED (letter)
   * if more than one key is pressed, turn off all letters
   * and leave them off until all keys are released.
   * The reason is that on the original enigma you can not press two keys at 
   * the same time since then the wheel rotation gets messed up
   */


  updateWheels();

  while (true) {
    if (checkWalzes()){
      updateWheels();
    }

    checkPlugboard();
    key=checkKB();
    if (key!=0){
      cnt=0;
      Serial.print(F("Key pressed: "));
      Serial.println(key);
      writeLetter(' ',1);

      if (key < -10){
	writeLetter('-',2);
      } else {
	writeLetter(' ',2);
      }

      if (key>=10 ){
         writeLetter(int(key/10)+'0',3);
      } else if (key <= -10 ){
	writeLetter(int((-key)/10)+'0',3);
      } else if (key < 0 ){
	writeLetter('-',3);
      } else {
         writeLetter(' ',3);
      }

      if (key >= 0 ){
	writeLetter(key%10+'0',4);
      } else {
	writeLetter((-key)%10+'0',4);
      }      
    } else { // no key pressed
      if ( cnt ==2 ){
	cnt++;
	updateWheels();
      } else {
	cnt++;
      }
    } // if key!=0;else

    if (HT.keysPressed()==1) {
      if (key==1){
	led=64;
	if (settings.currentWalze[2] == sizeof(walzeContent)-2) {  // index start at 0 and is terminated with \0
	  settings.currentWalze[2] = 0;
	} else {
	  settings.currentWalze[2]++;
	}
	updateWheels();
      } else if (key == 2){
	led=65;
	if (settings.currentWalze[1] == sizeof(walzeContent)-2) {  // index start at 0 and is terminated with \0
	  settings.currentWalze[1] = 0;
	} else {
	  settings.currentWalze[1]++;
	}
	updateWheels();
      } else if (key == 14){
	if (settings.currentWalze[0] == sizeof(walzeContent)-2) {  // index start at 0 and is terminated with \0
	  settings.currentWalze[0] = 0;
	} else {
	  settings.currentWalze[0]++;
	}
	updateWheels();
      }
      if (key==1 | key==2){
	ledOn=led;
	Serial.print(F("  turning on LED: "));
	HT.setLedNow(led);
	Serial.println(led);
	delay(111);
      }
    }
    if (HT.keysPressed()!=1){
      if (ledOn!=0){
	Serial.print(F("  turning off LED: "));
	HT.clearLedNow(led);
	Serial.println(led);
	ledOn=0;
      }
#ifdef disabled
    } else {
      HT.readKeyRaw(keys,false); // false to go from cache - what was read above
      Serial.print(F(" "));
      for (i = 0; i < 3; i++) {
	if (keys[i] < 0x1000) {
	  Serial.print(F("0"));
	}
	if (keys[i] < 0x100)  {
	  Serial.print(F("0"));
	}
	if (keys[i] < 0x10)   {
	  Serial.print(F("0"));
	}
	Serial.print(keys[i], HEX);
	Serial.print(F(" "));
      }

      Serial.print(F(" -> keys pressed "));
      Serial.println(HT.keysPressed());
#endif
    }
    //PSDEBUG: heartbeat
    i=!i;
    digitalWrite(13,i);
    delay(200);
  }
} // loop
