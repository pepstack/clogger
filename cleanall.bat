@echo off

echo Current directory: %CD%

echo === clean all projects ===
echo.

echo clean: build
rmdir /s /q "%~dp0build" 2>nul

echo clean: msvc/build
rmdir /s /q "%~dp0msvc\build" 2>nul

echo clean: msvc/.vs
rmdir /s /q "%~dp0msvc\.vs" 2>nul

echo.
echo === clean all projects done ===
pause
