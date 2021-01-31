/////////////////////////////////////////////////////////////////////
//
// ftcSoundBar arduino library example
//
// This example shows the usage of the ftcSoundBar library.
//
// Please connect to your ftcSoundBar's WEB-UI, and set I2C mode!
// 
// 5V-Boards (nano, ftDuino): please use a level shifter to connect
//                            your arduino with the ftcSoundBar!
//
// (C) 2021 Oliver Schmiel, Christian Bergschneider, Stefan Fuss
//
/////////////////////////////////////////////////////////////////////

#include "ftcSoundBar.h"

// first declare an instance of class FtcSoundBar
FtcSoundBar ftcSoundBar;

void setup() {

  // initialize serial interface
  while (!Serial);
  Serial.begin( 115200 );

  // no special setup procedure needed for ftcSounfBar
  
}

void loop() {

  Serial.println("play track #9");
  ftcSoundBar.play(9);
  delay(3000);

  Serial.println("pause");
  ftcSoundBar.pause();
  delay(3000);

  Serial.println("resume");
  ftcSoundBar.resume();
  delay(3000);

  Serial.println("next");
  ftcSoundBar.next();
  delay(3000);

  Serial.println("previous");
  ftcSoundBar.previous();
  delay(3000);

  Serial.println("stop");
  ftcSoundBar.stop();
  delay(3000);

  Serial.println("set mode shuffle");
  ftcSoundBar.setMode( MODE_SHUFFLE );
  delay(3000);

  Serial.println("set mode repeat");
  ftcSoundBar.setMode( MODE_REPEAT );
  delay(3000);
  
  Serial.println("set mode normal");
  ftcSoundBar.setMode( MODE_NORMAL );
  delay(3000);
  
  Serial.print("getVolume=");
  uint8_t vol = ftcSoundBar.getVolume();
  Serial.println( vol );
  delay(3000);

  Serial.println("set volume 10");
  ftcSoundBar.setVolume( 10 );
  delay(3000);

  Serial.println("set volume 20");
  ftcSoundBar.setVolume( 20 );
  delay(3000);

  Serial.println("set volume 30");
  ftcSoundBar.setVolume( 30 );
  delay(3000);

  Serial.println("set original volume");
  ftcSoundBar.setVolume( vol );
  delay(3000);

  Serial.println("*** restarting in 10 s ***");
  for (int i=0; i<10; i++ ) {
    Serial.print(".");
  }
  Serial.println();

}
