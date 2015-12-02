/****************************************************************
   MyEnigma
   v0.00 - test of library, keyboard and LEDs
*/

#include "ht16k33.h"
#include "asciifont.h"
#include <EEPROM.h>
#include <util/crc16.h>
#include <Wire.h>

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
typedef struct {
  uint8_t fwVersion;         // firmware version
  uint8_t preset;            // Allow multiple saved settings
  enigmaModel_t model;
  uint8_t ukw;               // what reflector that is loaded
  uint8_t rotor[4];          // current wheel no in the 4 or 3 positions
  char ringstellung[4];      // Setting of the 4 rotors ring.
  char plugboard[26][2];     // 26 to allow external Uhr box connected, then A->B doesn't mean B->A.
  unsigned long odometer;    // How many characters this unit has en/decrypted
  uint8_t currentrotor[4];   // current position of the 4 rotors
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
#define IODIRA 0x00 // IO Direction Register Address of Port A
#define IODIRB 0x01 // IO Direction Register Address of Port B
#define GPPUA  0x06 // Register Address of Port A
#define GPPUB  0x07 // Register Address of Port B
#define IOCON  0x0A // Control register
#define GPIOA  0x12 // Register Address of Port A
#define GPIOB  0x13 // Register Address of Port B
#define OLATA  0x14 // Register Address of Outputlatch A
#define OLATB  0x15 // Register Address of Outputlatch B


//Enigma lamp and Keyboard layout:
//   Q W E R T Z U I O
//    A S D F G H J K
//   P Y X C V B N M L

//The order the keys on the keyboard are coded
//Keyboard scancode table
//A-Z for keyboard, 1-9 for buttons

const char scancodes[36] = "QWERTZUIOASDFGHJKPYXCVBNML123456789";

//A=65 so the [0] element contain the number of the LED that shows "A"
//after 'Z'-65 comes control LEDs [\]^_
//Usage: led['A'-65] or led[char-65]
const byte led[] = {16, 22, 4, 17, 19, 25, 20, 8, 14, 0, 18, 3, 5, 6, 7, 9, 10, 15, 24, 23, 2, 21, 1, 13, 12, 11, 26, 27, 28, 29, 30};

/****************/
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
} // saveSettings

/****************************************************************/
void setup() {
  uint8_t i, j;
  unsigned long ccsum;
  unsigned char *ptr = (unsigned char *)&settings;

  Serial.begin(115200);
  Serial.println("My enigma v0.01");
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
    settings.rotor[0] = 0;
    settings.rotor[1] = 1;
    settings.rotor[2] = 2;
    settings.rotor[3] = 3;
    settings.ringstellung[0] = ' ';
    settings.ringstellung[1] = 'A';
    settings.ringstellung[2] = 'A';
    settings.ringstellung[3] = 'A';
    for (i = 0; i < sizeof(settings.plugboard)/2; i++) {
      settings.plugboard[i][0] = ' ';
      settings.plugboard[i][1] = ' ';
    }
    settings.currentrotor[0] = ' ';
    settings.currentrotor[1] = 'A';
    settings.currentrotor[2] = 'B';
    settings.currentrotor[3] = 'C';
    settings.odometer = 0;
    settings.nextlocation=0;
    settings.valid=true;
    settings.checksum = getCsum((void*) ptr, sizeof(settings) - 2);
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
  Serial.print(F("Rotors: "));
  Serial.print(settings.rotor[0], DEC);
  Serial.print(F(" "));
  Serial.print(settings.rotor[1], DEC);
  Serial.print(F(" "));
  Serial.print(settings.rotor[2], DEC);
  Serial.print(F(" "));
  Serial.println(settings.rotor[3], DEC);
  Serial.print(F("ringstellung: "));
  Serial.print(settings.ringstellung[0]);
  Serial.print(F(" "));
  Serial.print(settings.ringstellung[1]);
  Serial.print(F(" "));
  Serial.print(settings.ringstellung[2]);
  Serial.print(F(" "));
  Serial.println(settings.ringstellung[3]);
  Serial.print(F("Plugboard (Steckerbrett): "));
  for (i = 0; i < sizeof(settings.plugboard)/2; i++) {
    if (settings.plugboard[i][0] != ' ') {
      Serial.print(i, DEC);
      Serial.print(F(" "));
      Serial.print(settings.plugboard[i][0]);
      Serial.print(F("->"));
      Serial.print(settings.plugboard[i][1]);
      Serial.print(F(" "));
    }
  }
  Serial.println();
  Serial.print(F("Odo meter: "));
  Serial.println(settings.odometer, DEC);

  Wire.begin(); // enable the wire lib
  i2c_write(mcp_address,IOCON,0b00110100);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(no addr inc)+DISSLW(Slew rate disabled)+HAEN(disable hw addr)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write(mcp_address,IODIRA,0xff); // Set all ports to inputs
  i2c_write(mcp_address,IODIRB,0xff); // Set all ports to inputs
  i2c_write(mcp_address,GPPUA,0xff); // disable pullup (for now,to save power)
  i2c_write(mcp_address,GPPUB,0xff); //

  //The other chip
  i2c_write(mcp_address+1,IOCON,0b00110100);   // Init value for IOCON, bank(0)+INTmirror(no)+SQEOP(no addr inc)+DISSLW(Slew rate disabled)+HAEN(disable hw addr)+ODR(INT open)+INTPOL(act-low)+0(N/A)
  i2c_write(mcp_address+1,IODIRA,0xff); // Set all ports to inputs
  i2c_write(mcp_address+1,IODIRB,0xff); // Set all ports to inputs
  i2c_write(mcp_address+1,GPPUA,0xff); // disable pullup (for now,to save power)
  i2c_write(mcp_address+1,GPPUB,0xff); //

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

#ifdef TEST
  // Test the screen
  Serial.println(F("All LEDs on"));
  for (i = 0; i <= 128; i++) {
    HT.setLedNow(i);
    //    HT.sendLed();
    delay(500);
    Serial.print(F("set LED:"));
    Serial.println(i,DEC);
  }
  HT.sendLed();
  delay(500);

  Serial.println(F("All LEDs OFF"));
  for (i = 0; i <= 128; i++) {
    HT.clearLedNow(i);
    //HT.sendLed();
    // delay(100);
    Serial.print(F("clear LED:"));
    Serial.println(i,DEC);
  }
  HT.sendLed();

#endif

  // Clear decimal point
  for (i = 10; i <= 13; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, HIGH);
  }
} // setup

/****************************************************************/
uint8_t checkKB() {
  boolean ready;
  uint8_t i,keyflag;
  int8_t key;
  HT16K33::KEYDATA oldkeys;
  

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
void writeLetter(char letter, uint8_t rotor) {
  uint8_t led;
  int8_t i;
  uint16_t val;

  led = (rotor - 1) * 16;
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
// Write a single byte
uint8_t i2c_write(uint8_t unitaddr,uint8_t val1,uint8_t val2){
  Wire.beginTransmission(unitaddr);
  Wire.write(val1);
  Wire.write(val2);
  return Wire.endTransmission();
}

/****************************************************************/
// read a byte from specific address (send one byte(address to read) and read a byte)
 uint8_t i2c_read(uint8_t unitaddr,uint8_t addr){
   i2c_write(unitaddr,addr,addr);
  Wire.requestFrom(unitaddr,(uint8_t) 1);
  return Wire.read();    // read one byte
}



/****************************************************************/
// check the Steckerbrett
// update the plugboard array with what plugs that are connected

uint8_t checkPlugboard() {
  uint8_t plug;

  // make all io ports inputs-pullup
  i2c_write(mcp_address,GPPUA,0xff); // enable 100k pullup
  i2c_write(mcp_address,GPPUB,0xff); //
  i2c_write(mcp_address+1,GPPUA,0xff); // enable 100k pullup
  i2c_write(mcp_address+1,GPPUB,0xff); //
  
  for (plug=0;plug<sizeof(settings.plugboard)/2;plug++){
    //make port "plug" output
    //set  port "plug" low (all others are pullup=high)
    //read all registers, see if any other port is low
    //  if yes - update plug table and print out
    //make port input pullup
  }

} // checkPlugboard

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
  checkKB();

//  writeString("HELLO WORLD    ", 400);

  writeString(hello, 400);
  delay(500);
  writeString("V001",0);
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


  while (true) {
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
      if ( cnt >2 ){
	writeLetter(' ',1);
	writeLetter(' ',2);
	writeLetter(' ',3);
	writeLetter(' ',4);
      } else {
	cnt++;
      }
    } // if key!=0;else

    if (HT.keysPressed()==1) {
      if (key==1){
	led=64;
      } else if (key == 2){
	led=65;
      } else if (key == 14){
	led=66;
      }
      if (key==1 | key==2 | key==14){
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
