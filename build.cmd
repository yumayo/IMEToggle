@echo off
setlocal
docker compose -f "%~dp0compose.yml" up -d --build dev
if errorlevel 1 exit /b 1
docker compose -f "%~dp0compose.yml" exec -T dev sh /usr/local/bin/imetoggle-build
exit /b %errorlevel%
