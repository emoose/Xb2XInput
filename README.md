## Xb2XInput - user-mode Xbox OG controller driver for Windows

Xb2XInput is a small application that can translate the input from an Xbox OG controller over to a virtual XInput/DirectInput device for games to make use of, without needing any unsigned drivers to be installed!

While there's already solutions such as [XBCD](https://www.s-config.com/xbcd-original-xbox-controllers-win10/) or the [signed Mayflash driver](https://www.s-config.com/xbcd-mayflash-xbox-joystick-driver/) for this, unfortunately neither included support for my Madcatz controller out-of-the-box.  
With those my only solution was to edit the driver files, breaking any signature on them and forcing me to run Windows with unsigned driver support enabled...

Keeping unsigned drivers enabled is a bit too risky for me though, and I also found those drivers have some issues of their own too (a controller driver shouldn't be able to cause BSoDs!).  
Both of those only support DirectInput too, requiring another third-party app to translate that to XInput (with the best solution I found needing the app to be installed per-game...)

I decided to try looking into a user-mode solution instead, something with a smaller chance of taking my whole system down at will, and ideally something that supports both Xbox OG controllers and XInput.  
Sadly there doesn't seem to be much work done on something like that for Windows, but luckily others have worked on user-mode solutions for other kinds of controllers, which wasn't too hard to adapt to the Xbox OG controllers.  
(major thanks to MTCKC for their [ProconXInput project](https://github.com/MTCKC/ProconXInput/)!)

While it's not fully user-mode as it needs the ScpVBus driver to be installed, that driver does have its advantages over the other Xbox dedicated ones:

- Fully open source
- Used & tested by tons of people
- Not tied to Xbox - can be used to add support for pretty much any kind of controller
- Signed!

If you have ScpToolkit installed (eg for using DS3/DS4 controllers) you should already have ScpVBus installed too, otherwise the guide below explains how to set it up.

Setup
---
Download the latest compiled version of Xb2XInput from the [releases page](https://github.com/emoose/Xb2XInput/releases).

**To make use of Xb2XInput you'll need the ScpVBus driver installed, and your controller will need to be setup to use the "WinUSB" driver.**

The included "install driver.bat" file will install the ScpVBus driver for you, just extract the entire zip somewhere and run that as administrator (right-click -> Run as administrator), make sure to run it from the correct folder for your OS (x86 for 32-bit machines, x64 for 64-bit)

Setting up the controller for WinUSB is a little more complicated:
1. Make sure the controller is plugged in.
2. Open Device Manager and locate the device.
3. Right-click the device and select "Update driver" from the context menu.
4. In the wizard, select "Browse my computer for driver software"
5. Select "Let me pick from a list of device drivers on my computer"
6. From the list of device classes, select "Universal Serial Bus devices"
7. The wizard should now display "WinUsb Device" on the left, click on it, and then choose "WinUsb Device" from the list on the right.
8. A warning might appear about Windows not recommending this driver etc, click "Yes" to continue installing the driver.
9. If all went well it should say that the "WinUsb Device" was installed successfully.

After both drivers are setup you should restart your computer to make sure they take effect.

Usage
---
With the drivers setup, just extract the Xb2XInput-x.x.zip file somewhere (making sure the libusb-1.0 and XOutput1_1 DLLs are next to Xb2XInput.exe!)

Now run the Xb2XInput.exe and an icon for it should appear in your system tray.  

##### Connecting a controller
- To connect a controller just plug it into your system, after a few seconds Xb2XInput should detect it and a notification will appear once it starts translating it over to XInput.  

##### Disconnecting a controller
- Similarly, unplugging a controller will show a notification about the controller being disconnected.

##### Viewing status
- To view the status of Xb2XInput just hover over the icon, if only one controller is being translated it'll list details about that controller, otherwise if there's more than 1 it'll give the number of controllers connected.

##### Run on startup
- To run Xb2XInput on startup just click the icon and choose the "Run on startup" option, a registry entry will be made for Xb2XInput to be ran from it's current path.  
If you move the Xb2XInput exe (and associated dlls) make sure to choose the "Run on startup" option again to update the startup path.

##### Exit
- To exit the application just click the icon and choose Exit, any connected gamepads will stop translating to XInput.

##### Support
- If you have any problems with Xb2XInput please don't hesitate to make an issue on the issue tracker, just be sure to include the hardware ID of your device (can be found in Device Manager -> double-click device -> Details -> Hardware IDs, should look something like "USB\VID_0738&PID_4522&REV_0384")

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

Thanks to nefarius for [ScpVBus/XOutput](https://github.com/nefarius/ScpVBus) (part of the awesome [ScpToolkit](https://github.com/nefarius/ScpToolkit) project).

Big thanks to the [libusb](https://libusb.info/) developers too.

Xbox icon was taken from starvingartist's [Antiseptic Videogame Systems icon set](https://www.deviantart.com/starvingartist/art/Antiseptic-Videogame-Systems-23217105)

(No thanks to MS for requiring 3rd-party tools to use an OG controller, but their signed WinUSB driver is nice I guess)

Contributing
---
Contributions & improvements would be gladly accepted, ideally you should make any changes in a new branch seperate to master, and then create a pull request from that branch.

This way if I update any code while you're working, you should be able to merge any changes in pretty easily.

Todo
---
- Allow setting deadzones / calibrating the analog sticks
- Make a script for the WinUSB driver installation?
- Test multiple Xbox OG controllers at once (waiting for one to be delivered as we speak)

License
---
"install/uninstall driver.bat" and XOutput.cpp/XOutput.hpp is taken from [ProconXInput](https://github.com/MTCKC/ProconXInput/) which is MIT (Expat) licensed, rest of the code is licensed under GPLv3.
