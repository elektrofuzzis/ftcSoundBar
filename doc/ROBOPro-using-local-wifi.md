# TXT with ROBOPro using a local wifi connection

In this configuration, you use the local wifi to send commands from your TXT controller to the ftcSoundBar. Since you already connected the ftcSoundBar to your local wifi during setup, it's a very simple way to program the ftcSoundBar. 

The wifi connection is nice, if you are working on a bigger projekt, because you don't need any cabling between TXT and ftcSoundBar at all.

**a) upload libftcSoundBar.so on your TXT controller**

First, you need to upload the `libftcSoundBar.so` library to your TXT controller. 
* Connect your TXT controller to your local wifi network using client mode (SETTINGS/NETWORK/WLAN/WLAN MODE/WLAN-CLIENT).
* Enable SSH DAEMON and WEB SERVER on your TXT controller (SETTINGS/SECURITY).
* Reboot your TXT controller.
* Start a browser and enter the TXT's IP address to connect to TXT's web server.
* The TXT will ask for simple authentification. Username is ROBOPro and and password is your TXT's serial number.
* The web site will display a directoy. Choose folder `libs`.
* Use button `upload files` to upload `libftcSoundBar.so` library into the `libs` folder.

If you get an write error while uploading the library, you need to change some rights on your TXT controller. This is a bug, dependend on your TXT's firmware version. You need to change the rights only one time:
* Connect to your TXT controller using a ssh client like  [putty](https://www.putty.org/).
* Username is ROBOPro, password is ROBOPro, too.
* Run `chmod 744 /opt/knobloch/libs`.

The step uploading the library is a little bit tricky. But never mind, you need to repeat this step after deploying a new firmware version only.

**b) Check ftcSoundBar**

* Connect the ftcSoundBar to your local wifi network as described in chapter 2.
* Use [http://ftcSoundBar](http://ftcSoundBar) to checkout your sound files track numbers. Since ROBOPro isn't able to handle strings, you need the track numbers to write your code.

**c) Working with ROBOPro**

* Download ftcSoundBarWifi.rpp and store it in your ROBOPro's library folder.
* Checkout the ROBOPro examples ftcSoundBarPlayer.rpp and ftcSoundBar_wifi_Example1.rpp
* A detailed description of the ROBOPro API is described in ...

The ROBOPro library is just a facade of the shared library `libftcSoundBar.so`. In standard wifi configuration, the library tries to access ftcSoundBar using it's DNS name `ftcSoundBar`, which is resolved by mDNS from the ftcSoundBar itself. Since TXT is connected to your local wifi and you already managed to access the ftcSoundBar's WEB UI, this procedure is very robust.

This procedure will fail, if ftcSoundBar and TXT controler are not connected to the same IP segment. In this case add the ftcSoundBar's ip-address to your local DNS controller or use the IP-commands in the robopro library.

**Remarks**

* MAC-Users: Please be aware, there a some limitations in using ROBOPro on a MAC. Wine won't work with this configuration at all, Parallels is fine but a little bit slow.
* Working with USB cable: You could connect your PC with the TXT using a USB cable as well. The TXT is able to be connected to multiple networks at on time (wifi to access ftcSoundBar and USB to conect to ROBOPro).
* Offline Mode is working fine, too. 