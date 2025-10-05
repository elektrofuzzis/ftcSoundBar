# TXT 4.0 controller using ROBOProCoding & I2C

To connect the ftcSoundBar via i²c-bus, you need [6-to-pin-adapter](https://www.fischerfriendswoman.de/index.php?p=7&sp=9#R7668al) and two 1:1 ribbon cables between TXT 4.0 extention port and ftcSoundBar's I2C interface at the right side. 

- Checkout `RoboProCoding` folder
- Try `ftcSoundBar.ft` to play some songs.

*The getter functions like getVolume are not stable, due to some I2C bus limitations in the used expressif framework. As a workaoud, you could store the last transferred value in a global variable and use it instead of calling the getter functions.*