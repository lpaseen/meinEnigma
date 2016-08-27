/****************************************************************
 *  PlugboardTest
 *  Simple program to test the plugboard functionality
*/


#include <Wire.h>

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

//How the plugboard is mapped
//[0] is first input port on first mcp23017
// (char)pgm_read_byte(&steckerbrett[0]+key-1)
//with steckerbrett as A-Z the plugboard ends up as
// YVSPMJGDA
//  WTQNKHEB
// ZXUROLIFC
// or
//  Q   W   E   R   T   Z   U   I   O
// 24  21  18  15  12  09  06  03  00
//
//    A   S   D   F   G   H   J   K
//   22  19  16  13  10  07  04  01
//
//  P   Y   X   C   V   B   N   M   L
// 25  23  20  17  14  11  08  05  02
//
//                                   00000000001111111111222222
//                                   01234567890123456789012345
//const byte steckerbrett[] PROGMEM = "OKLIJMUHNZGBTFVRDCESXWAYQP"; // 
const byte steckerbrett[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 
//

/****************************************************************/
///
/// Write a single byte to i2c
///
uint8_t i2c_write(uint8_t unitaddr,uint8_t val){
  Wire.beginTransmission(unitaddr);
  Wire.write(val);
  return Wire.endTransmission();
}

/****************************************************************/
///
///Write two bytes to i2c
///
uint8_t i2c_write2(uint8_t unitaddr,uint8_t val1,uint8_t val2){
  Wire.beginTransmission(unitaddr);
  Wire.write(val1);
  Wire.write(val2);
  return Wire.endTransmission();
}

/****************************************************************/
///
/// read a byte from specific address (send one byte(address to read) and read a byte)
///
uint8_t i2c_read(uint8_t unitaddr,uint8_t addr){
  i2c_write(unitaddr,addr);
  Wire.requestFrom(unitaddr,1);
  return Wire.read();    // read one byte
}

/****************************************************************/
///
/// read 2 bytes starting at a specific address
/// read is done in two sessions - as required by mcp23017 (page 5 in datasheet)
///
uint16_t i2c_read2(uint8_t unitaddr,uint8_t addr){
  uint16_t val;
  i2c_write(unitaddr,addr);
  Wire.requestFrom(unitaddr, 1);
  val=Wire.read();
  Wire.requestFrom(unitaddr, 1);
  return Wire.read()<<8|val;
}

/****************************************************************/
/****************************************************************/
void setup() {

  Serial.begin(38400);
  Serial.println(F("Plugboard test v0.01"));
  Serial.println();

  
  Wire.begin(); // enable the wire lib

  //Check for plugboard
  Wire.beginTransmission(mcp_address);

  if (Wire.endTransmission()==0){
    Serial.println(F("Preparing plugboard"));
    // Setup the port multipler
    i2c_write2(mcp_address,IOCON,0b00011110);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
    i2c_write2(mcp_address,IODIRA,0xff); // Set all ports to inputs
    i2c_write2(mcp_address,IODIRB,0xff); // Set all ports to inputs
    i2c_write2(mcp_address,GPPUA,0xff); // enable pullup, seems to sometimes be false readings otherwise and guessing to slow on pullup
    i2c_write2(mcp_address,GPPUB,0xff); //

    //The other chip
    i2c_write2(mcp_address+1,IOCON,0b00011110);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(addr inc)+DISSLW(Slew rate disabled)+HAEN(hw addr always enabled)+ODR(INT open)+INTPOL(act-low)+0(N/A)
    i2c_write2(mcp_address+1,IODIRA,0xff); // Set all ports to inputs
    i2c_write2(mcp_address+1,IODIRB,0xff); // Set all ports to inputs
    //  i2c_write2(mcp_address+1,GPPUA,0); // disable pullup (for now,to save power)
    //  i2c_write2(mcp_address+1,GPPUB,0); //
    i2c_write2(mcp_address+1,GPPUA,0xff); // enable pullup, seems to sometimes be a problem otherwise
    i2c_write2(mcp_address+1,GPPUB,0xff); //
  }else{
    Serial.println(F("No plugboard found"));
  }

} // setup


void printHex4(uint16_t val){
  if (val<0x1000){Serial.print(F("0"));};
  if (val<0x100){Serial.print(F("0"));};
  if (val<0x10){Serial.print(F("0"));};
  Serial.print(val,HEX);
} //printHex4

void loop(){
  uint16_t val1,val2;


  //get all values back
  printHex4(i2c_read2(mcp_address,GPIOA));
  Serial.print(F(" "));
  printHex4(i2c_read2(mcp_address+1,GPIOA));
  Serial.println();
  delay(500);
} // loop
