@echo off
"%~dp0.venv\Scripts\python.exe" -u "%~dp0gdocs.py" %*
exit /b %errorlevel%
