@echo off
rem Compila el cliente con AddressSanitizer en out\build\asan (carpeta aparte; NO es el Main.exe que se publica).
rem Uso: tools\build_asan.bat   -> out\build\asan\src\Release\Main.exe + clang_rt.asan_dynamic-x86_64.dll
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
if not exist out\build\asan\CMakeCache.txt (
  cmake -S . -B out\build\asan -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE=toolchain-x64.cmake -DENABLE_EDITOR=OFF "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /bigobj /fsanitize=address /Zi" "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /bigobj /fsanitize=address /Zi" "-DCMAKE_EXE_LINKER_FLAGS=/INCREMENTAL:NO /DEBUG" "-DCMAKE_SHARED_LINKER_FLAGS=/INCREMENTAL:NO /DEBUG" 2>&1
  if errorlevel 1 exit /b 1
)
cmake --build out\build\asan --config Release 2>&1
