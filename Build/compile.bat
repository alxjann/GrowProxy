@echo off
cmake -S "%~dp0.." -B "%~dp0Proxy" && cmake --build "%~dp0Proxy" --config Release --target Proxy
pause
