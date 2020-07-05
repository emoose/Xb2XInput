@echo off
goto start

::What this .bat does-

::Checks for administrator permissions by seeing if we can use `net session`
::then installs ScpVBus

:start
echo Administrative permissions required. Checking permissions...
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Administrative permissions denied. Launch as an Administrator!
    goto exit
)

cd %~dp0

echo Installing WinUSB drivers for known gamepads (this might take a minute or two)
wdi-simple --vid 0x044F --pid 0x0F07 --type 0 --name "Thrustmaster Controller"
wdi-simple --vid 0x045E --pid 0x0202 --type 0 --name "Microsoft Xbox Controller v1 (US)"
wdi-simple --vid 0x045E --pid 0x0285 --type 0 --name "Microsoft Xbox Controller S (Japan)"
wdi-simple --vid 0x045E --pid 0x0287 --type 0 --name "Microsoft Xbox Controller S"
wdi-simple --vid 0x045E --pid 0x0288 --type 0 --name "Microsoft Xbox Controller S v2"
wdi-simple --vid 0x045E --pid 0x0289 --type 0 --name "Microsoft Xbox Controller v2 (US)"
wdi-simple --vid 0x046D --pid 0xCA84 --type 0 --name "Logitech Cordless Precision"
wdi-simple --vid 0x046D --pid 0xCA88 --type 0 --name "Logitech Thunderpad"
wdi-simple --vid 0x05FD --pid 0x1007 --type 0 --name "Mad Catz Controller (unverified)"
wdi-simple --vid 0x05FD --pid 0x107A --type 0 --name "InterAct PowerPad Pro X-box pad"
wdi-simple --vid 0x05FE --pid 0x3030 --type 0 --name "Chic Controller"
wdi-simple --vid 0x05FE --pid 0x3031 --type 0 --name "Chic Controller"
wdi-simple --vid 0x062A --pid 0x0020 --type 0 --name "Logic3 Xbox GamePad"
wdi-simple --vid 0x06A3 --pid 0x0201 --type 0 --name "Saitek Adrenalin"
wdi-simple --vid 0x0738 --pid 0x4506 --type 0 --name "MadCatz 4506 Wireless Controller"
wdi-simple --vid 0x0738 --pid 0x4516 --type 0 --name "MadCatz Control Pad"
wdi-simple --vid 0x0738 --pid 0x4520 --type 0 --name "MadCatz MC2 Racing Wheel and Pedals"
wdi-simple --vid 0x0738 --pid 0x4526 --type 0 --name "MadCatz Control Pad Pro"
wdi-simple --vid 0x0738 --pid 0x4536 --type 0 --name "MadCatz MicroCON"
wdi-simple --vid 0x0738 --pid 0x4556 --type 0 --name "MadCatz Lynx Wireless Controller"
wdi-simple --vid 0x0738 --pid 0x4586 --type 0 --name "MadCatz MicroCon Wireless Controller"
wdi-simple --vid 0x0738 --pid 0x4588 --type 0 --name "MadCatz Blaster"
wdi-simple --vid 0x0C12 --pid 0x0005 --type 0 --name "Intec wireless"
wdi-simple --vid 0x0C12 --pid 0x8801 --type 0 --name "Nyko Xbox Controller"
wdi-simple --vid 0x0C12 --pid 0x8802 --type 0 --name "Zeroplus Xbox Controller"
wdi-simple --vid 0x0C12 --pid 0x880A --type 0 --name "Pelican Eclipse PL-2023"
wdi-simple --vid 0x0C12 --pid 0x8810 --type 0 --name "Zeroplus Xbox Controller"
wdi-simple --vid 0x0C12 --pid 0x9902 --type 0 --name "HAMA VibraX - "FAULTY HARDWARE""
wdi-simple --vid 0x0E4C --pid 0x1097 --type 0 --name "Radica Gamester Controller"
wdi-simple --vid 0x0E4C --pid 0x2390 --type 0 --name "Radica Games Jtech Controller"
wdi-simple --vid 0x0E4C --pid 0x3240 --type 0 --name "Radica Gamester"
wdi-simple --vid 0x0E4C --pid 0x3510 --type 0 --name "Radica Gamester"
wdi-simple --vid 0x0E6F --pid 0x0003 --type 0 --name "Logic3 Freebird wireless Controller"
wdi-simple --vid 0x0E6F --pid 0x0005 --type 0 --name "Eclipse wireless Controller"
wdi-simple --vid 0x0E6F --pid 0x0006 --type 0 --name "Edge wireless Controller"
wdi-simple --vid 0x0E6F --pid 0x0008 --type 0 --name "After Glow Pro Controller"
wdi-simple --vid 0x0F30 --pid 0x010B --type 0 --name "Philips Recoil"
wdi-simple --vid 0x0F30 --pid 0x0202 --type 0 --name "Joytech Advanced Controller"
wdi-simple --vid 0x0F30 --pid 0x8888 --type 0 --name "BigBen XBMiniPad Controller"
wdi-simple --vid 0x102C --pid 0xFF0C --type 0 --name "Joytech Wireless Advanced Controller"
wdi-simple --vid 0x0738 --pid 0x4522 --type 0 --name "MadCatz LumiCON"
wdi-simple --vid 0xFFFF --pid 0xFFFF --type 0 --name "PowerWave Xbox Controller"

echo Driver installation complete!
:exit
pause
