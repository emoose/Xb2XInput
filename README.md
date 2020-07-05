## Xb2XInput - user-mode Xbox OG controller driver for Windows

Xb2XInput is a small application that can translate the input from an Xbox OG controller over to a virtual XInput/DirectInput device for games to make use of, without needing any unsigned drivers to be installed!

While there's already solutions such as [XBCD](https://www.s-config.com/xbcd-original-xbox-controllers-win10/) or the [signed Mayflash driver](https://www.s-config.com/xbcd-mayflash-xbox-joystick-driver/) for this, unfortunately neither included support for my Madcatz controller out-of-the-box.  
With those my only solution was to edit the driver files, breaking any signature on them and forcing me to run Windows with unsigned driver support enabled...

Keeping unsigned drivers enabled is a bit too risky for me though, and I also found those drivers have some issues of their own too (a controller driver shouldn't be able to cause BSoDs!).  
Both of those only support DirectInput too, requiring another third-party app to translate that to XInput (with the best solution I found needing the app to be installed per-game...)

I decided to try looking into a user-mode solution instead, something with a smaller chance of taking my whole system down at will, ideally something that supports both Xbox OG controllers and XInput.  
Sadly there doesn't seem to be much work done on something like that for Windows, but luckily others have worked on user-mode solutions for other kinds of controllers, which wasn't too hard to adapt to the Xbox OG controllers.  
(major thanks to MTCKC for their [ProconXInput project](https://github.com/MTCKC/ProconXInput/)!)

While it's not fully user-mode as it needs the ViGEmBus driver to be installed, ViGEmBus does have its advantages over the other drivers:

- Fully open source
- Used & tested by tons of people
- Not tied to Xbox - can be used to add support for pretty much any kind of controller
- Signed!

Setup
---
Download the latest compiled version of Xb2XInput from the [releases page](https://github.com/emoose/Xb2XInput/releases).

To make use of Xb2XInput you'll need the ViGEmBus driver installed, this can be downloaded from here: https://github.com/ViGEm/ViGEmBus/releases (If you're a Windows 7 user, make sure to install the prerequisites listed on that page!)

Once ViGEmBus has been installed, the controllers will need to be setup to use the "WinUSB" driver, the included "install drivers.bat" can take care of setting up these drivers for you.

Simply extract the entire zip somewhere, navigate to your OS's directory (x86 for 32-bit, x64 for 64-bit) and then run the "install drivers.bat" as administrator (right-click -> Run as administrator).

Once the batch file is completed you should restart your computer to make sure they take effect.

To uninstall/remove the WinUSB driver you'll have to uninstall the driver from your gamepad in device manager (make sure "Delete the driver software for this device" is checked)

Usage
---
With the drivers setup, just extract the Xb2XInput-x.x.zip file somewhere (making sure the libusb-1.0 DLL is next to Xb2XInput.exe!)

Now run the Xb2XInput.exe and an icon for it should appear in your system tray.  

#### Connecting a controller
To connect a controller just plug it into your system, after a few seconds Xb2XInput should detect it and a notification will appear once it starts translating it over to XInput.  

#### Disconnecting a controller
Similarly, unplugging a controller will show a notification about the controller being disconnected.

#### Viewing status
To view the status of Xb2XInput just hover over the icon, any details about connected devices should be shown in the tooltip (if tooltip doesn't appear, click the icon instead)

#### Guide button emulation
Sadly the Xbox OG controller doesn't contain the guide button usually found on XInput devices. However as of Xb2XInput v1.3.2 this button can now be emulated, through the use of the LT+RT+LS+RS button combination.

By default this button combination will be enabled, but if desired you can easily disable it through the system tray menu, in case it interferes with something else you need the combination for.

(right now you'll have to disable the combination manually each time XB2X is ran, but hopefully in future we can store your preference somewhere instead)

#### Deadzone settings
Similarly to the Guide button emulation, use the LT+RT+(LS/RS)+DPAD Up/Dn combinaiton for analog stick deadzone and (LT/RT)+LS+RS+DPAD Up/Dn for trigger deadzone adjustment. The deadzone may be set individually on each controller. The current deadzones will be displayed in the context menu each time it is displayed (not live updating). For example, to increase the left analog stick deadzone radius use the button combination LT+RT+LS+DPAD Up (Keep pressing DPAD Up while holding LT+RT+LS to increase by increments of 500). A setting of 3500-4000 works well on my XBOX Controller v2 (US), but ymmv. Xb2XInput does remap the remaining useable range of the axes from origin to max extent. To disable the deadzone button combination, deselect the option through the system tray menu. The deadzone will remain active if the button combination is disabled.

(right now you'll have to set the deadzone manually each time XB2X is ran, but hopefully in future we can store your preference somewhere instead)

#### Vibration toggle
In case you wish to disable your controllers vibration function, eg. if a game has issues with it, or your controller has problems with the motors, you can also do this through the context-menu.

(As with the guide button emulation toggle above, your choice isn't saved yet unfortunately, so you will have to disable it manually each time XB2X is ran)

#### Run on startup
To run Xb2XInput on startup just click the icon and choose the "Run on startup" option, a registry entry will be made for Xb2XInput to be ran from it's current path.  
If you move the Xb2XInput exe (and associated dlls) make sure to choose the "Run on startup" option again to update the startup path.

#### Exit
To exit the application just click the icon and choose Exit, any connected gamepads will stop translating to XInput.

#### Support
If you have any problems with Xb2XInput please don't hesitate to make an issue on the issue tracker, just be sure to include the hardware ID of your device (can be found in Device Manager -> double-click device -> Details -> Hardware IDs, should look something like "USB\VID_0738&PID_4522&REV_0384")

Supported controllers
---
Xb2XInput should support all the controllers that XBCD has support for, minus any steering wheels/DDR pads.  
(no wheel/DDR support since I don't have any and I'm not sure how they translate to XInput, if anyone has one and can connect it to their PC I'd be happy to try debugging it with you though, just make an issue on the issue tracker!)

For a list of all supported controllers see the top section of [XboxController.cpp](https://github.com/emoose/Xb2XInput/blob/master/Xb2XInput/XboxController.cpp)

While a controller might be supported that doesn't mean it's been tested or works, the majority should hopefully all work without problem, but there could be some edge cases.

If you have any issues, or have a controller that isn't on this list, please make an issue on the issues page (including the hardware ID, see Support section above)

Thanks
---
MTCKC's [ProconXInput project](https://github.com/MTCKC/ProconXInput/) (for supporting Nintendo Switch Pro Controllers) was invaluable in creating Xb2XInput.

Thanks to nefarius & ViGEm team for [ViGEmBus](https://github.com/ViGEm/ViGEmBus) (part of the awesome [ViGEm](https://github.com/ViGEm) project).

Big thanks to the [libusb](https://libusb.info/) developers too.

Xbox icon was taken from starvingartist's [Antiseptic Videogame Systems icon set](https://www.deviantart.com/starvingartist/art/Antiseptic-Videogame-Systems-23217105)

wdi-simple.exe taken from the pbatard's [libwdi project](https://github.com/pbatard/libwdi)

(No thanks to MS for requiring 3rd-party tools to use an OG controller, but their signed WinUSB driver is nice I guess)

Contributing
---
Contributions & improvements would be gladly accepted, ideally you should make any changes in a new branch seperate to master, and then create a pull request from that branch.

This way if I update any code while you're working, you should be able to merge any changes in pretty easily.

Todo
---
- Allow setting deadzones / calibrating the analog sticks
- Test x86 version

License
---
"install/uninstall driver.bat" is taken from [ProconXInput](https://github.com/MTCKC/ProconXInput/) which is MIT (Expat) licensed.

ViGEmClient code taken from [ViGEmClient](https://github.com/ViGEm/ViGEmClient), licensed under MIT license.

wdi-simple taken from [libwdi project](https://github.com/pbatard/libwdi) which is LGPL 3 licensed.

The rest of the code is licensed under GPLv3.
