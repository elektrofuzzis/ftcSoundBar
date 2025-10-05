# Build the Software

If you don't want, you don't need to build the software by your own. In every release, you could download the needed prebuild files.

The following text describes how to build your own release.

## ftcSoundBar's firmware

To flash the ftcSoundBar firmware the first time, you need to install some tools on your PC.

1. `git clone -b v5.2 https://github.com/espressif/esp-idf.git`
2. `cd esp-idf`
3. run `install.bat` on windows systems or `install.sh` on linux distributions
4. `git clone -b v2.7 https://github.com/espressif/esp-adf.git`
5. set in esp-idf/export.bat on windows Systems or export.sh on linux systems the variable ADF_PATH. Use the path, where you installed espadf.
6. `git clone https://github.com/elektrofuzzis/ftcSoundBar.git`
7. run `esp-idf/export.bat` or `esp-idf/export.sh` dependend on your OS.
8. `cd ftcSoundBar/firmware`
9. `idf.py build`
10. `idf.py flash`

## .so-library - TXT controller only

1. Download and install gcc compiler arm-linux-gnueabihf from <a href="https://www.linaro.org/downloads/">linareo<a>
2. `arm-linux-gnueabihf-gcc.exe -Ilib\gcc\arm-linux-gnueabihf\4.9.1\include -c -fpic -Wall libftcSoundBar.cpp`
3. `arm-linux-gnueabihf-gcc.exe -Ilib\gcc\arm-linux-gnueabihf\4.9.1\include -shared libftcSoundBar.o`

