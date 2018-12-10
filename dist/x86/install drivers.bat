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
cd Drivers
echo Installing system drivers...

devcon install .\ScpVBus\ScpVBus.inf Root\ScpVBus

:exit
pause
