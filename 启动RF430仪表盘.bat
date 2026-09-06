@echo off
cd /d "%~dp0"
python tools\serial_dashboard.py
if errorlevel 1 pause
