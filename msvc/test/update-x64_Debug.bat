@ECHO OFF

echo "copy: pthreadVC2.dll => %1"
copy /Y "%~dp0..\..\deps\pthreads-w32\Pre-built.2\dll\x64\pthreadVC2.dll" "%1"

echo "copy: pthreadGC2.dll => %1"
copy /Y "%~dp0..\..\deps\pthreads-w32\Pre-built.2\dll\x64\pthreadGC2.dll" "%1"
