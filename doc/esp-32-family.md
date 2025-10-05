# ESP32 using i2c and C++

esp-32-controllers have an i²c-interface and an wifi interface as well. The actual implementation uses the i²c-inteface. But feel free to implement the wifi RESTapi.

* All GPIO-ports of your esp-32-controller could be used as i²C-interface. Connect SDA, SCL and GND with the ftcSoundBar i²c interface.
* You don't need a evel shifter, since both boards are based on 3.3V logic levels.
* Download `ftcSoundBar.zip` from this repo.
* Add this library to your Arduino IDE using Sketch/Include File/Add zip-library.
* You might add the correct SDA and SCL ports to the library.
* Checkout the example application via FILE/EXAMPLES/FTCSOUNDBAR/FTCSOUNDBARDUINO
* A detailed description of the Arduino API is described in ...

**Remarks:**
* See hardware description of the ftcSoundBar's i²c connector.