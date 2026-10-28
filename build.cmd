@echo off
rem Compila todo y copia los ejecutables listos para usar a host\ y cliente\.
rem Requiere Visual Studio 2022 o más nuevo (o Build Tools) con "Desarrollo para el escritorio con C++".
setlocal
set ROOT=%~dp0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set VS=
rem (No se usa for /f: el ")" de "Program Files (x86)" corta el comando.)
rem Solo una instalación que tenga el compilador de C++.
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\halocoop_vs.txt" 2>nul
set /p VS=<"%TEMP%\halocoop_vs.txt"
del "%TEMP%\halocoop_vs.txt" 2>nul
if not defined VS (
    echo.
    echo Visual Studio with "Desktop development with C++" was not found.
    echo Install it as explained in README.md, section "Building from source".
    goto :failed
)
echo Building... the first time takes a few minutes.
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || goto :failed
cmake -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=Release || goto :failed
cmake --build "%ROOT%build" || goto :failed
copy /y "%ROOT%build\HaloCoopHost.exe" "%ROOT%host\HaloCoopHost.exe" >nul || goto :copy_failed
copy /y "%ROOT%build\VERSION.dll" "%ROOT%host\VERSION.dll" >nul || goto :copy_failed
copy /y "%ROOT%build\HaloCoopClient.exe" "%ROOT%cliente\HaloCoopClient.exe" >nul || goto :copy_failed
echo.
echo DONE: host\HaloCoopHost.exe, host\VERSION.dll and cliente\HaloCoopClient.exe are up to date.
call :pause_if_double_clicked
exit /b 0

:copy_failed
echo.
echo Could not copy the files: close HaloCoopHost.exe and HaloCoopClient.exe and try again.
:failed
echo.
echo BUILD FAILED. Read the messages above.
call :pause_if_double_clicked
exit /b 1

rem Con doble clic la ventana se cerraría sin dejar ver el resultado.
:pause_if_double_clicked
echo %CMDCMDLINE% | find /i "%~f0" >nul && pause
exit /b 0
