@echo off
rem Compila el cliente. Siempre en Release: el unico Main.exe que debe existir es
rem out\build\windows-x64\src\Release\Main.exe (no compilar RelWithDebInfo ni Debug).
tasklist /fi "imagename eq Main.exe" | find /i "Main.exe" >nul
if not errorlevel 1 (
    echo El cliente esta abierto y bloquea sus archivos: cierralo y vuelve a ejecutar.
    exit /b 1
)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0..\out\build\windows-x64"
cmake --build . --config Release 2>&1
