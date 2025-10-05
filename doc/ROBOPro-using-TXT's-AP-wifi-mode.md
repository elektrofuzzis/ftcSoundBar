# TXT with ROBOPro using a TXT in AP mode

Using the TXT's AP wifi mode is nearby the same procedure as using local wifi. The advantage of this mode is running your model on an exhibition or convention; you don't need any public wifi infrastructure.

* upload the library `libftcSoundBar.so` as described in 3.2.1.
* Change your TXT wifi settings to AP MODE.
* Connect to your ftcSoundBar's setup page, activate switch "TXT Client Mode", set TXT's SSID & password and save configuration.

Now you could access the ftcSoundBar from your TXT using the TXT's AP Mode. Please keep in mind, that you can't access to the WEB-UI of the ftcSoundBar from your PC any more. To access it, you need to connect your notebook to the TXT wifi, too. Since you might not get an ip-address assigned autmatically, you might setup a static ip-address on your PCs wifi card.

To connect the TXT from your PC, it's helpful to use the USB cable.

The ROBOPro library is the same as in local wifi mode. So all of your ROBOPro programs working in local wifi mode will run in AP mode, too.