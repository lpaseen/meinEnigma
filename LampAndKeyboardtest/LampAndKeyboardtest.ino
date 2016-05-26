/****************************************************************
   Simple test of ht16k33 library both led and keyboard
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
//A-Z for keyboard, 0-3 for buttons under walze 0-3
//(char)pgm_read_byte(&scancodes[0]+key-1)
//protoboard const char scancodes[] PROGMEM = "OIUZTREWQGHJKLMNBVCXYPASDF0123";
//Prod board
const char scancodes[] PROGMEM = "QWERTZUIOASDFPYXCVBNMLGHJK0123";

//A=65 so the [0] element contain the number of the LED that shows "A"
//after 'Z'-65 comes control LEDs [\]^_
//Usage: led['A'-65] or led[char-65]
//        pgm_read_byte(led+'A'-65));
//Protoboard const byte led[] PROGMEM = {12, 22, 24, 14, 9, 15, 28, 29, 4, 30, 31, 19, 20, 21, 3, 27, 11, 8, 13, 7, 5, 23, 10, 25, 26, 6};
//Prod board
//                           A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q   R   S   T   U   V   W   X   Y   Z
const byte led[] PROGMEM = {12, 24, 22, 14,  5, 15, 28, 29, 10, 30, 31, 27, 26, 25, 11, 19,  3,  6, 13,  7,  9, 23,  4, 21, 20,  8};
//  Q   W   E   R   T   Z   U   I   O
//  3   4   5   6   7   8   9  10  11
//
//    A   S   D   F   G   H   J   K
//   12  13  14  15  28  29  30  31
//
//  P   Y   X   C   V   B   N   M   L
// 19  20  21  22  23  24  25  26  27


/****************************************************************/
void setup() {
  uint8_t led;
  Serial.begin(38400);
  Serial.println("Lamp and keyboard test v0.01");
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
  for (led = 0; led < 128; led++) {
    HT.clearLed(led);
  } // for led
  HT.sendLed();
}

/****************************************************************/
void loop() {
  static uint8_t prevLed = 0;
  static uint8_t lastKey = 0, ledOn;
  static int8_t dir = 1;
  int8_t key;
  static int16_t lastKeyCode = 0;

  key = HT.readKey();
  if (key != 0) {
    if (key != lastKey) {
      if (key < 0) {
        lastKeyCode = pgm_read_byte(&scancodes[0] + (-key) - 1);
      } else {
        lastKeyCode = pgm_read_byte(&scancodes[0] + key - 1);
      }

      Serial.print(F("Key # "));
      Serial.print(key);
      Serial.print(F(" = "));
      Serial.print((char)lastKeyCode);

      if (key > 0) {
//        ledOn = pgm_read_byte(led + ((byte)pgm_read_byte(&scancodes[0] + key - 1) - 'A'));
        ledOn = pgm_read_byte(led + lastKeyCode - 'A');
        Serial.print(F(", turning on LED: "));
        Serial.print(ledOn);
        HT.setLedNow(ledOn);
        delay(11);
      } else {
        HT.clearLedNow(ledOn);
      }
      lastKey = key;
      Serial.println();
    }
  }
  delay(100);
}
