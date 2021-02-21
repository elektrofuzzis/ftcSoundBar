/////////////////////////////////////////////////////////////////////
//
// ftcSoundBar arduino library
//
// Version 1.31
//
// (C) 2021 Oliver Schmiel, Christian Bergschneider, Stefan Fuss
//
/////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include "ftcSoundBar.h"
#include <Arduino.h>

FtcSoundBar::FtcSoundBar( uint8_t myI2CAddress = 0x33 ) {
  // constructor
  Wire.begin();
  I2CAddress = myI2CAddress;
}

void FtcSoundBar::i2cSend( i2c_cmd_t cmd ) {
  Wire.beginTransmission( I2CAddress );
  Wire.write( cmd );
  Wire.endTransmission();
}

void FtcSoundBar::i2cSend( i2c_cmd_t cmd, uint8_t data ) {
  Wire.beginTransmission( I2CAddress );
  Wire.write( cmd );
  Wire.write( data );
  Wire.endTransmission();  
}

uint8_t FtcSoundBar::i2cReceive( i2c_cmd_t cmd ) {

  i2cSend( (uint8_t) cmd, 1 );
  delay( 100 );
  Wire.requestFrom( I2CAddress, (uint8_t)1);
  while ( !Wire.available());
  return Wire.read(); 
}

    
void FtcSoundBar::play( uint8_t track ) {
  // play Track;
  i2cSend( I2C_CMD_PLAY, track );
}

void FtcSoundBar::setVolume( uint8_t volume ) {
  // set volume
  i2cSend( I2C_CMD_SET_VOLUME, volume );
}

uint8_t FtcSoundBar::getVolume( void ) {
  // get volume
  return i2cReceive( I2C_CMD_GET_VOLUME );
}

void FtcSoundBar::stop( void ) {
  // stop running track
  i2cSend( I2C_CMD_STOP_TRACK );
}

void FtcSoundBar::pause( void ) {
  // pause running track
  i2cSend( I2C_CMD_PAUSE_TRACK );
}

void FtcSoundBar::resume( void ) {
  // resume running track
  i2cSend( I2C_CMD_RESUME_TRACK );
}

void FtcSoundBar::setMode( play_mode_t mode ) {
  // set mode
  i2cSend( I2C_CMD_SET_MODE, (uint8_t) mode );
}

play_mode_t FtcSoundBar::getMode( void ) {
  // get mode
  return (play_mode_t) i2cReceive( I2C_CMD_GET_MODE );
}

uint8_t FtcSoundBar::getTracks( void ) {
  // get tracks
  return i2cReceive( I2C_CMD_GET_TRACKS );
}

uint8_t FtcSoundBar::getActiveTrack( void ) {
  // get active track
  return i2cReceive( I2C_CMD_GET_ACTIVE_TRACK );
}

state_t FtcSoundBar::getTrackState( void ) {
  // get track state
  return (state_t) i2cReceive( I2C_CMD_GET_TRACK_STATE );
}

void FtcSoundBar::next( void ) {
  // play next track
  i2cSend( I2C_CMD_NEXT );
}

void FtcSoundBar::previous( void ) {
  // play previous track
  i2cSend( I2C_CMD_PREVIOUS );
}
