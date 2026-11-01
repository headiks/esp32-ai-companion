@echo off
rem Double-click to start. Uses the Python from PATH: a plain .py double-click goes to the py launcher's default version instead.
cd /d "%~dp0"
python emulator\main.py %*
if errorlevel 1 pause
