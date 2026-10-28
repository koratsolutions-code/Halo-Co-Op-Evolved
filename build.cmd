@echo off
rem Compila todo y copia los ejecutables listos para usar a host\ y cliente\.
rem Requiere Visual Studio 2022 (o Build Tools) con "Desarrollo para el escritorio con C++".
setlocal
set ROOT=%~dp0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set VS=
rem (No se usa for /f: el ")" de "Program Files (x86)" corta el comando.)
"%VSWHERE%" -latest -products * -property installationPath > "%TEMP%\halocoop_vs.txt" 2>nul
set /p VS=<"%TEMP%\halocoop_vs.txt"
del "%TEMP%\halocoop_vs.txt" 2>nul
if not defined VS (
    echo No se encontro Visual Studio 2022.
    exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%ROOT%build" || exit /b 1
copy /y "%ROOT%build\HaloCoopHost.exe" "%ROOT%host\HaloCoopHost.exe" >nul || exit /b 1
copy /y "%ROOT%build\HaloCoopClient.exe" "%ROOT%cliente\HaloCoopClient.exe" >nul || exit /b 1
echo Listo: host\HaloCoopHost.exe y cliente\HaloCoopClient.exe
