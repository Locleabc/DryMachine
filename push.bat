@echo off
rem Day code len GitHub: https://github.com/Locleabc/DryMachine
rem Bam dup chuot. Lan dau Git se mo trinh duyet de dang nhap GitHub.
cd /d "%~dp0"
set "REPO=%~dp0"
set "REPO=%REPO:~0,-1%"
set "REPO=%REPO:\=/%"
git config --global --get-all safe.directory | findstr /x /c:"%REPO%" >nul || git config --global --add safe.directory "%REPO%"
git remote get-url origin >nul 2>&1 || git remote add origin https://github.com/Locleabc/DryMachine.git
git push -u origin main
echo.
pause
