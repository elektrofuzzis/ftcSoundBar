# TXT with ROBOPro using a I2C cable

The fischertechnik TX controller doesn't have a wifi card. So you need to work with a ribbon cable. Since TX is a 5V controller, you need to use a level shifter like [ftExtender](https://gundermann-software.de/produkt/ft-extender/) between TX and ftcSoundBar.

- Checkout `robopro` folder
- Try `ftcSoundBar_I2C_Example.rpp` to play some songs.
- Store `robopro/lib/ftcSoundBarI2C.rpp` in your ROBOPro's library folder, so you could use it in your own project.

*MAC-Users: `ftcSoundBarI2C.rpp` is working fine with Parallels and Wine.*.