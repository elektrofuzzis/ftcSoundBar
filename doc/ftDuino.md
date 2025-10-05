# ftDuino using I2C and C++

The ftDuino controller doesn't have a wifi card. So you need to work with i2C-bus. Since ftDuino is a 5V controller, you need to use a level shifter like [ftExtender](https://gundermann-software.de/produkt/ft-extender/) etween ftDuino and ftcSoundBar.

* Download `ftcSoundBar.zip` from this repo.
* Add this library to your Arduino IDE using Sketch/Include File/Add zip-library.
* Connect ftDuino's i2²c port with the ftExtender using a 1:1 ribbon cable. Use another 1:1 ribbon cable to connect ftExtender and ftcSoundBar. You could connect other i²c-devices with the same cable. Their bus addresses need to be different to 0x33, which is the ftcSoundBar's address.
* Checkout the example application via FILE/EXAMPLES/FTCSOUNDBAR/FTCSOUNDBARDUINO
* A detailed description of the Arduino API is described in ...
