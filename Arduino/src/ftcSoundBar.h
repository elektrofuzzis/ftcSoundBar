/////////////////////////////////////////////////////////////////////
//
// ftcSoundBar arduino library
//
// Version 1.3
//
// (C) 2021 Oliver Schmiel, Christian Bergschneider, Stefan Fuss
//
/////////////////////////////////////////////////////////////////////

#ifndef ftcSoundBar_h
#define ftcSoundBar_h

// ftcSoundBar modes to use with setMode()
typedef enum {
  MODE_NORMAL = 0,
  MODE_SHUFFLE = 1,
  MODE_REPEAT = 2
} play_mode_t;

// ftcSoundBar states to use with getState()
typedef enum {
    STATE_NONE = 0,
    STATE_INIT,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_STOPPED,
    STATE_FINISHED,
    STATE_ERROR
} state_t;

// internal definition of I2C-CMDs.
typedef enum I2C_CMD {
  I2C_CMD_PLAY=0,
  I2C_CMD_SET_VOLUME=1,
  I2C_CMD_GET_VOLUME=2,
  I2C_CMD_STOP_TRACK=3,
  I2C_CMD_PAUSE_TRACK=4,
  I2C_CMD_RESUME_TRACK=5,
  I2C_CMD_SET_MODE=6,
  I2C_CMD_GET_MODE=7,
  I2C_CMD_GET_TRACKS=8,
  I2C_CMD_GET_ACTIVE_TRACK=9,
  I2C_CMD_GET_TRACK_STATE=10,
  I2C_CMD_NEXT=11,
  I2C_CMD_PREVIOUS=12
} i2c_cmd_t;

class FtcSoundBar {
  private:
    uint8_t I2CAddress;
    void i2cSend( i2c_cmd_t cmd );
    void i2cSend( i2c_cmd_t cmd, uint8_t data );
    uint8_t i2cReceive( i2c_cmd_t cmd );
  public:
    FtcSoundBar( uint8_t myI2CAddress = 0x33 );
      // constructor
    void play( uint8_t track );
      // play Track;
    void setVolume( uint8_t volume );
      // set volume
    uint8_t getVolume( void ); 
      // get volume
    void stop( void ); 
      // stop running track
    void pause( void ); 
      // pause running track
    void resume( void );
      // resume running track
    void setMode( play_mode_t mode ); 
      // set mode
    play_mode_t getMode( void ); 
      // get mode
    uint8_t getTracks( void ); 
      // get tracks
    uint8_t getActiveTrack( void ); 
      // get active track
    state_t getTrackState( void ); 
      // get track state
    void next( void ); 
      // play next track
    void previous( void ); 
      // play previous track
};

#endif
