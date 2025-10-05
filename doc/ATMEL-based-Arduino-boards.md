# Arduino using I2C and C++

Most Arduino controllers don't have a wifi card. So you need to work with i2C-bus. Since most of them are 5V controllers, you need to use a level shifter like [ftExtender](https://gundermann-software.de/produkt/ft-extender/) between your Ardiono and ftcSoundBar.

* Download `ftcSoundBar.zip` from this repo.
* Add this library to your Arduino IDE using Sketch/Include File/Add zip-library.
* Connect your Arduino's i2²c port with the 6 pin port of the ftExtender. Use a 1:1 ribbon cable to connect the 10 pin port of the ftExtender and ftcSoundBar. You could connect other i²c-devices with the same cable. Their bus addresses need to be different to 0x33, which is the ftcSoundBar's address.
* Checkout the example application via FILE/EXAMPLES/FTCSOUNDBAR/FTCSOUNDBARDUINO
* A detailed description of the Arduino API is described in ...

**Remarks:**
* See chapter 1 for a description of the ftcSoundBar's i²c connector.
* See ftExtender's documentation for a description of the i²c connectors.