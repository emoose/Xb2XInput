@echo off
goto start

::What this .bat does-

::Checks for administrator permissions by seeing if we can use `net session`
::then uninstalls ViGEmBus & ScpVBus

:start
echo Administrative permissions required. Checking permissions...
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Administrative permissions denied. Launch as an Administrator!
    goto exit
)

cd %~dp0
cd Drivers
echo Removing system drivers...

devcon remove Root\ViGEmBus
devcon remove Root\ScpVBus

:exit
pause
