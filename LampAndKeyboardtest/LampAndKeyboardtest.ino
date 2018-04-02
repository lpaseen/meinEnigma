/****************************************************************
   Simple test of ht16k33 library both led and keyboard

   Usage: Upload this program and then watch the serial console
   When you press a key the serial console shows what key is
   pressed and also light up the corresponding LED
   release the key and the LED goes off.



*/

#include "ht16k33.h"

// Define the class
HT16K33 HT;

//Enigma lamp and Keyboard layout:
//   Q W E R T Z U I O
//    A S D F G H J K
//   P Y X C V B N M L

//The order the keys on the keyboard are coded
//Keyboard scancode table
// "scancodes[]" is an array to map a key to a letter
// A-Z for keyboard, 0-3 for buttons under walze 0-3
//(char)pgm_read_byte(&scancodes[0]+key-1)
//protoboard const char scancodes[] PROGMEM = "OIUZTREWQGHJKLMNBVCXYPASDF0123";
//Prod board
const char scancodes[] PROGMEM = "QWERTZUIOASDFPYXCVBNMLGHJK0123";

//"led[]" is an array to map a letter to a led number
//A=65 so the [0] element contain the number of the LED that shows "A"
//after 'Z'-65 comes control LEDs [\]^_
//Usage: led['A'-65] or led[char-65]
//        pgm_read_byte(led+'A'-65));
//Protoboard const byte led[] PROGMEM = {12, 22, 24, 14, 9, 15, 28, 29, 4, 30, 31, 19, 20, 21, 3, 27, 11, 8, 13, 7, 5, 23, 10, 25, 26, 6};
//Prod board
//                           A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q   R   S   T   U   V   W   X   Y   Z
const byte led[] PROGMEM = {12, 24, 22, 14,  5, 15, 28, 29, 10, 30, 31, 27, 26, 25, 11, 19,  3,  6, 13,  7,  9, 23,  4, 21, 20,  8};


/****************************************************************/
void setup() {
  uint8_t led;
  Serial.begin(38400);
  Serial.println("Lamp and keyboard test v0.02");
  Serial.println();
  // initialize everything, 0x00 is the i2c address for the first chip (0x70 is added in the class).
  HT.begin(0x00);

  // flash the LEDs, first turn them on
  Serial.println(F("Turn on all LEDs"));
  for (led = 0; led < 128; led++) {
    HT.setLed(led);
  } // for led
  HT.sendLed();
  delay(1000);
  //Next clear them
  Serial.println(F("Clear all LEDs"));
  HT.clearAll();
  Serial.println();
  Serial.println(F("Press any key and it will show up below"));
  Serial.println(F("cnt:    Keyno = Letter,            LED"));
}

/****************************************************************/
void loop() {
  static uint8_t prevLed = 0;
  static uint8_t lastKey = 0, ledOn, ledOff;
  static int8_t dir = 1;
  int8_t key;
  static int16_t lastKeyCode = 0;
  uint8_t keyCnt;

  key = HT.readKey(); // read what key is recently pressed down or released
  if (key != 0) { // something is changed
    if (key != lastKey) { //same key as last time ?
      if (key < 0) { // figure out what letter that is pressed/released
        lastKeyCode = pgm_read_byte(&scancodes[0] + (-key) - 1);
      } else {
        lastKeyCode = pgm_read_byte(&scancodes[0] + key - 1);
      }
      keyCnt = HT.keysPressed(); // How many keys are pressed down ?
      if (keyCnt == 0) // none left, clear the board
        HT.clearAll();

      if (keyCnt < 10)
        Serial.print(F(" "));
      Serial.print(keyCnt, DEC);
      Serial.print(F(": Key # "));
      if (key >= 0)
        Serial.print(F(" "));
      if (key > -10 && key < 10 )
        Serial.print(F(" "));
      Serial.print(key);
      Serial.print(F(" = "));
      Serial.print((char)lastKeyCode);

      if (key > 0) { // if a new key was pressed, turn on cirresponding led
        ledOn = pgm_read_byte(led + lastKeyCode - 'A'); // get what led to turn on from the "led" array
        Serial.print(F(", turning on  LED: "));
        if (ledOn < 10)
          Serial.print(F(" "));
        Serial.print(ledOn);
        HT.setLedNow(ledOn);
        delay(11);
      } else {
        ledOff = pgm_read_byte(led + lastKeyCode - 'A');
        Serial.print(F(", turning off LED: "));
        if (ledOff < 10)
          Serial.print(F(" "));
        Serial.print(ledOff);
        HT.clearLedNow(ledOff);
      }
      lastKey = key;
      Serial.println();
    }
  }
  delay(100);
}
