/****************************************************************
    PlugboardTest
    Simple program to test the plugboard functionality
*/


#include <Wire.h>

//  MCP23017 registers, all as seen from bank0
//
#define mcp_address 0x20 // I2C Address of MCP23017
#define mcp_address2 0x24 // I2C Address of MCP23017
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

#define BIT(val, i) ((val >> i) & 1)

static const uint8_t pbLookup[52][3] = { // could make it PROGMEM but then the code below gets bigger instead
  //mcp=i2c address,port=0 or 1,bit=0 to 7
  {mcp_address, 0, 0},
  {mcp_address, 0, 1},
  {mcp_address, 0, 2},
  {mcp_address, 0, 3},
  {mcp_address, 0, 4},
  {mcp_address, 0, 5},
  {mcp_address, 0, 6},
  {mcp_address, 0, 7},
  {mcp_address, 1, 0},
  {mcp_address, 1, 1},
  {mcp_address, 1, 2},
  {mcp_address, 1, 3},
  {mcp_address, 1, 4},
  {mcp_address, 1, 5},
  {mcp_address, 1, 6},
  {mcp_address, 1, 7},
  {mcp_address + 1, 0, 0},
  {mcp_address + 1, 0, 1},
  {mcp_address + 1, 0, 2},
  {mcp_address + 1, 0, 3},
  {mcp_address + 1, 0, 4},
  {mcp_address + 1, 0, 5},
  {mcp_address + 1, 0, 6},
  {mcp_address + 1, 0, 7},
  {mcp_address + 1, 1, 0},
  {mcp_address + 1, 1, 1},
  {mcp_address2, 0, 0},
  {mcp_address2, 0, 1},
  {mcp_address2, 0, 2},
  {mcp_address2, 0, 3},
  {mcp_address2, 0, 4},
  {mcp_address2, 0, 5},
  {mcp_address2, 0, 6},
  {mcp_address2, 0, 7},
  {mcp_address2, 1, 0},
  {mcp_address2, 1, 1},
  {mcp_address2, 1, 2},
  {mcp_address2, 1, 3},
  {mcp_address2, 1, 4},
  {mcp_address2, 1, 5},
  {mcp_address2, 1, 6},
  {mcp_address2, 1, 7},
  {mcp_address2 + 1, 0, 0},
  {mcp_address2 + 1, 0, 1},
  {mcp_address2 + 1, 0, 2},
  {mcp_address2 + 1, 0, 3},
  {mcp_address2 + 1, 0, 4},
  {mcp_address2 + 1, 0, 5},
  {mcp_address2 + 1, 0, 6},
  {mcp_address2 + 1, 0, 7},
  {mcp_address2 + 1, 1, 0},
  {mcp_address2 + 1, 1, 1}
};

uint16_t plugA[4];

//How the plugboard is mapped
//[0] is first input port on first mcp23017
// (char)pgm_read_byte(&steckerbrett[0]+key-1)
//with steckerbrett as A-Z the plugboard ends up as
// ABCDEFGHI
//  JCKLMNOPQ
// QRSTUVWXYZ
// or
//  Q   W   E   R   T   Z   U   I   O
//  0   1   2   3   4   4   6   7   8
//
//    A   S   D   F   G   H   J   K
//    9  10  11  12  13  14  15  16
//
//  P   Y   X   C   V   B   N   M   L
// 17  18  19  20  21  22  23  24  25
//
//
//                             00000000001111111111222222
//                             01234567890123456789012345
const byte steckerbrett[] PROGMEM = "QERTYZUIOASDFGHJKPYXCVBNML"; //
//const byte steckerbrett[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; //

/****************************************************************/
///
/// Write a single byte to i2c
///
uint8_t i2c_write(uint8_t unitaddr, uint8_t val) {
  Wire.beginTransmission(unitaddr);
  Wire.write(val);
  return Wire.endTransmission();
}

/****************************************************************/
///
///Write two bytes to i2c
///
uint8_t i2c_write2(uint8_t unitaddr, uint8_t val1, uint8_t val2) {
  Wire.beginTransmission(unitaddr);
  Wire.write(val1);
  Wire.write(val2);
  return Wire.endTransmission();
}

/****************************************************************/
///
/// read a byte from specific address (send one byte(address to read) and read a byte)
///
uint8_t i2c_read(uint8_t unitaddr, uint8_t addr) {
  i2c_write(unitaddr, addr);
  Wire.requestFrom(unitaddr, 1);
  return Wire.read();    // read one byte
}

/****************************************************************/
///
/// read 2 bytes starting at a specific address
/// read is done in two sessions - as required by mcp23017 (page 5 in datasheet)
///
uint16_t i2c_read2(uint8_t unitaddr, uint8_t addr) {
  uint16_t val;
  i2c_write(unitaddr, addr);
  Wire.requestFrom(unitaddr, 1);
  val = Wire.read();
  Wire.requestFrom(unitaddr, 1);
  return Wire.read() << 8 | val;
}

/****************************************************************/
///
/// print a 16bit hex number with leading 0
///
void printHex4(uint16_t val) {
  if (val < 0x1000) {
    Serial.print(F("0"));
  };
  if (val < 0x100) {
    Serial.print(F("0"));
  };
  if (val < 0x10) {
    Serial.print(F("0"));
  };
  Serial.print(val, HEX);
} //printHex4


/****************************************************************/
///
/// print a 16bit binary number with leading 0 and space at 8 bits.
///
void printBin16(uint16_t val) {
  int8_t bitno;
  for (bitno = 15; bitno >= 0; bitno--) {
    if (bitRead(val, bitno)) {
      Serial.print(F("1"));
    } else {
      Serial.print(F("0"));
    }
    if (bitno == 8 || bitno == 0)
      Serial.print(F(" "));
  }
} //printBin16


/****************************************************************/
///
//Set a pin output and low
///
void setPortOut(uint8_t plug) {
  //make port "plug" output
  i2c_write2(pbLookup[plug][0], IODIRA + pbLookup[plug][1], 0xff ^ (1 << pbLookup[plug][2]));
  //set  port "plug" low
  i2c_write2(pbLookup[plug][0], GPIOA + pbLookup[plug][1], 0xff ^ (1 << pbLookup[plug][2]));
}

/****************************************************************/
///
/// Set the port as input, will set other ports on same bus input also.
///
void setPortIn(uint8_t plug) {
  i2c_write2(pbLookup[plug][0], GPIOA + pbLookup[plug][1], 0xff);
  i2c_write2(pbLookup[plug][0], IODIRA + pbLookup[plug][1], 0xff);
}

/****************************************************************/
///
/// Read all plugs and load the plug array with the values
///
void readAll() {
  uint16_t val;
  plugA[0] = i2c_read2(mcp_address, GPIOA);
  plugA[1] = i2c_read2(mcp_address + 1, GPIOA);
  plugA[2] = i2c_read2(mcp_address2, GPIOA);
  plugA[3] = i2c_read2(mcp_address2 + 1, GPIOA);
  plugA[1] |= 0xf600; // set the unused pins also, just so bit count is easier
  plugA[3] |= 0xf600;
}

/****************************************************************/
/****************************************************************/
void setup() {
  boolean boardMissing;

  Serial.begin(38400);
  Serial.println(F("Multi plugboard test v0.10"));
  Serial.println();

  Wire.begin(); // enable the wire lib

  do {
    boardMissing = false;
    //Check for plugboard
    Wire.beginTransmission(mcp_address2);
    if (Wire.endTransmission() != 0) {
      Serial.println(F("Testing plugboard not found"));
      boardMissing = true;
    }

    Wire.beginTransmission(mcp_address);
    if (Wire.endTransmission() != 0) {
      Serial.println(F("Plugboard to test not found"));
      boardMissing = true;
    }
    delay(1000);
  } while (boardMissing);

  Serial.println(F("Preparing plugboard"));

  // Setup the port multipler
  i2c_write2(mcp_address2, IOCON, 0b00011110); // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address2, IODIRA, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address2, IODIRB, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address2, GPPUA, 0xff); // enable pullup, seems to sometimes be false readings otherwise and guessing to slow on pullup
  i2c_write2(mcp_address2, GPPUB, 0xff); //

  i2c_write2(mcp_address2 + 1, IOCON, 0b00011110); // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address2 + 1, IODIRA, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address2 + 1, IODIRB, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address2 + 1, GPPUA, 0xff); // enable pullup
  i2c_write2(mcp_address2 + 1, GPPUB, 0xff); //


  i2c_write2(mcp_address, IOCON, 0b00011110); // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address, IODIRA, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address, IODIRB, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address, GPPUA, 0xff); // enable pullup
  i2c_write2(mcp_address, GPPUB, 0xff); //

  i2c_write2(mcp_address + 1, IOCON, 0b00011110); // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write2(mcp_address + 1, IODIRA, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address + 1, IODIRB, 0xff); // Set all ports to inputs
  i2c_write2(mcp_address + 1, GPPUA, 0xff); // enable pullup
  i2c_write2(mcp_address + 1, GPPUB, 0xff); //

  // read current state of the pins
  readAll();
} // setup


/****************************************************************/
/****************************************************************/

#define SHOWBITS

void loop() {
  uint16_t val1, val2;
  uint8_t i, j;
  uint8_t errcnt, bitcnt;
  boolean plug[52];

  errcnt = 0;
  for (i = 0; i < 52; i++) {
    plug[i] = true;
  }

  for (i = 0; i < 52; i++) {
    setPortOut(i);
    readAll();
#ifdef SHOWBITS
    if (i < 10) {
      Serial.print(F(" "));
    }
    Serial.print(i);
    Serial.print(F(" : "));
    for (j = 0; j < 4; j++) {
      printBin16(plugA[j]); Serial.print(F(" "));
      //                printHex4(plugA[j]);Serial.print(F(" "));
    }
#endif
    bitcnt = (__builtin_popcount(plugA[0]) +
              __builtin_popcount(plugA[1]) +
              __builtin_popcount(plugA[2]) +
              __builtin_popcount(plugA[3]));
    /*
        if (i < 16) {
          if (BIT(plugA[2], i)) {
            plug[i] = true;
          }
        } else if (i < 26) {
          if (BIT(plugA[3], i - 16)) {
            plug[i] = true;
          }
        }
    */
    if (bitcnt != 62 ) {
#ifdef SHOWBITS
      Serial.print(F("<<<<<<<<<<<<<<<<<<<< "));
      Serial.print(bitcnt);
#endif
      errcnt++;
      if (plug < 26) {
        plug[i] = false;
      } else {
        plug[i - 26] = false;
      }
    }
#ifdef SHOWBITS
    Serial.println();
#endif
    //    delay(500);
    setPortIn(i);
  }

  //(char)pgm_read_byte(&steckerbrett[0]+key-1)
  //const byte steckerbrett[] = "JHGFDSAOIUZTREWQ      LMNBVCXYPK";
  for (i = 0; i < 26; i++) {
    if (plug[i]) {
      Serial.print(" ");
      Serial.print((char)toupper(pgm_read_byte(&steckerbrett[0] + i)));
      Serial.print("  ");
    } else {
      Serial.print(">");
      Serial.print((char)tolower(pgm_read_byte(&steckerbrett[0] + i)));
      Serial.print("< ");
    }
    if (i == 8) {
      Serial.println(); Serial.print(" ");
    } else if (i == 16) {
      Serial.println();
    }
  }
  Serial.println();

  if (errcnt > 0) {
    Serial.print(F("found "));
    Serial.print(errcnt);
    Serial.print(F(" error(s)"));
  }
  Serial.println();
  Serial.println();
  delay(2000);
  Serial.println('\f');
} // loop
