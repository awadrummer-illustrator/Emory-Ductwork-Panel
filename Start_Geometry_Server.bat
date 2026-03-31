@echo off
title Emory Ductwork - Geometry Server
cd /d "%~dp0python"
echo ========================================
echo   Emory Ductwork Geometry Server
echo ========================================
echo.
echo Watch folder: %TEMP%\mdux_watch
echo.
echo Keep this window open while using the Illustrator panel.
echo Press Ctrl+C to stop the server.
echo.
python geometry_server.py
pause

